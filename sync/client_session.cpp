#include <filesystem>
#include <fstream>
#include "client_session.hpp"
#include "../source/source_manager.hpp"
#include "../destination/destination_manager.hpp"
#include <iostream>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#endif
#include <cstring>

ClientSession::ClientSession(const std::string& ip, int port, int id)
    : ip_(ip), port_(port), sessionId_(id), socketFD_(-1), connected_(false) {}

ClientSession::~ClientSession() {
    close();
}

// connecting to the specified ip and port
bool ClientSession::connectToServer() {
    #ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0) {
        std::cerr << "[Session] WSAStartup failed" << std::endl;
        return false;
    }
    #endif

    socketFD_ = socket(AF_INET, SOCK_STREAM, 0);
    if (socketFD_ < 0) {
        #ifdef _WIN32
        std::cerr << "[Session] Socket creation failed: " << WSAGetLastError() << std::endl;
        #else
        perror("[Session] Socket creation failed");
        #endif
        return false;
    }

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port_);
    #ifdef _WIN32
    InetPtonA(AF_INET, ip_.c_str(), &serverAddr.sin_addr);
    #else
    inet_pton(AF_INET, ip_.c_str(), &serverAddr.sin_addr);
    #endif

    if (connect(socketFD_, (sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        #ifdef _WIN32
        std::cerr << "[Session] Connection failed: " << WSAGetLastError() << std::endl;
        closesocket(socketFD_);
        #else
        perror("[Session] Connection failed");
        ::close(socketFD_);
        #endif
        socketFD_ = -1;
        return false;
    }

    connected_ = true;
    std::cout << "[Session " << sessionId_ << "] Connected to " << ip_ << ":" << port_ << "\n";
    return true;
}

// communication with the server
void ClientSession::runTransaction(const std::string request,const std::string& localPath,const std::string&remotePath) {
    if (!connected_) {
        std::cerr << "[Session " << sessionId_ << "] Not connected.\n";
        return;
    }

    if(request=="push"){
        pushTransaction(localPath, remotePath);
    }else if(request=="pull"){
        pullTransaction(localPath, remotePath);
    }else{
        std::cerr << "[Session " << sessionId_ << "] Invalid request.\n";
    }

    // after each transaction close the client session
    std::cout << "[Session " << sessionId_ << "] Closing client session\n";
    connected_ = false;
    std::cout<<">>> "<<std::flush;
    #ifdef _WIN32
    closesocket(socketFD_);
    #else
    ::close(socketFD_);
    #endif
}

void ClientSession::close() {
    if (connected_) {
        #ifdef _WIN32
        closesocket(socketFD_);
        #else
        ::close(socketFD_);
        #endif
        connected_ = false;
        std::cout << "[Session " << sessionId_ << "] Connection closed.\n";
    }
}

bool ClientSession::isConnected() const {
    return connected_;
}

std::string ClientSession::getInfo() const {
    return "[Session " + std::to_string(sessionId_) + "] " + ip_ + ":" + std::to_string(port_) +
           (connected_ ? " [CONNECTED]" : " [DISCONNECTED]");
}

bool ClientSession::recievingStatus(DataTransfer &dataPipe) {
    // it returns true only when status is successfull
    // in all other cases it returns false and socket get closed
    // it prints message in all cases
    StatusMessage message(false, "Unknown error occurred");
    if (dataPipe.recieveStatus(socketFD_, message)) {
        std::cout << "\033[34m[Session " << sessionId_ << ": Server] " << message.msg << "\033[0m\n";
        std::cout<<">>> ";
        return message.status;
    } else {
        std::cerr << "\033[34m[Session " << sessionId_ << ": Server] Error while receiving status\033[0m\n";
        std::cout<<">>> ";
        return false;
    }
}
void printClientMessage(int sessionId, const std::string& message) {
    std::cout << "\033[36m[Session " << sessionId << ": Client] " << message << "\033[0m\n";
    std::cout<<">>> ";
}
bool ClientSession::pushTransaction(const std::string& loaclPath,const std::string& remotePath){
    DataTransfer dataPipe;
    std::string command = "PUSH\n";
    if (send(socketFD_, command.c_str(), command.size(), 0) != (ssize_t)command.size()) {
        printClientMessage(sessionId_,"Failed to send command");
        return false;
    }

    // Send destination file path (remotePath)
    if (!dataPipe.sendFilePath(socketFD_, remotePath)) {
        printClientMessage(sessionId_, "Failed to send destination file path");
        return false;
    }
    // Send source file name (for logging, not used for pathing)
    std::string sourceFileName;
    size_t pos = loaclPath.find_last_of("/\\");
    if (pos != std::string::npos)
        sourceFileName = loaclPath.substr(pos + 1);
    else
        sourceFileName = loaclPath;
    if (!dataPipe.sendFilePath(socketFD_, sourceFileName)) {
        printClientMessage(sessionId_, "Failed to send source file name");
        return false;
    }
    printClientMessage(sessionId_, "Push request sent to the remote machine");

    // Receive first status message from server (after receiving path)
    StatusMessage statusMsg1(false, "");
    if (!dataPipe.recieveStatus(socketFD_, statusMsg1)) {
        printClientMessage(sessionId_, "Failed to receive first status from server");
        return false;
    }
    
    if (!statusMsg1.status) {
        printClientMessage(sessionId_, "Server error: " + statusMsg1.msg);
        return false;
    }
    
    printClientMessage(sessionId_, "Server: " + statusMsg1.msg);

    // Receive second status message from server (after generating block hashes)
    StatusMessage statusMsg2(false, "");
    if (!dataPipe.recieveStatus(socketFD_, statusMsg2)) {
        printClientMessage(sessionId_, "Failed to receive second status from server");
        return false;
    }
    
    if (!statusMsg2.status) {
        printClientMessage(sessionId_, "Server error: " + statusMsg2.msg);
        return false;
    }
    
    printClientMessage(sessionId_, "Server: " + statusMsg2.msg);

    // Receive third status message from server (before sending block hashes)
    StatusMessage statusMsg3(false, "");
    if (!dataPipe.recieveStatus(socketFD_, statusMsg3)) {
        printClientMessage(sessionId_, "Failed to receive third status from server");
        return false;
    }
    
    if (!statusMsg3.status) {
        printClientMessage(sessionId_, "Server error: " + statusMsg3.msg);
        return false;
    }
    
    printClientMessage(sessionId_, "Server: " + statusMsg3.msg);

    // Receive block hashes from server
    std::vector<BlockInfo> blocks;
    if(dataPipe.receiveBlockHashes(socketFD_,blocks)){
        printClientMessage(sessionId_,"Hashes of "+std::to_string(blocks.size())+" blocks received");
    }else{
        printClientMessage(sessionId_,"Failed to receive block hashes");
        return false;
    }

    // Generate delta and send to server
    SourceManager source(loaclPath,blocks);
    printClientMessage(sessionId_,"Generating the delta instructions");
    Result<std::vector<DeltaInstruction>> deltaResult=source.getDelta(); 
    // this delta is need to be send over the network
    if(!deltaResult.success){
        printClientMessage(sessionId_,"Failed to Generate Delta: "+deltaResult.message);
        return false;
    }

    printClientMessage(sessionId_,"Generated " + std::to_string(deltaResult.data.size()) + " delta instructions");

    if(dataPipe.serializeAndSendDeltaInstructions(socketFD_,deltaResult.data)){
        printClientMessage(sessionId_,"Delta instructions send successfully");
    }else{
        printClientMessage(sessionId_,"Failed to send delta");
        return false;
    }

    if(!recievingStatus(dataPipe)) {
        printClientMessage(sessionId_,"Failed to receive delta received confirmation");
        return false; // whether deltas are successfully recieved or not
    }
    printClientMessage(sessionId_,"Server confirmed delta received");

    if(!recievingStatus(dataPipe)) {
        printClientMessage(sessionId_,"Failed to receive delta application confirmation");
        return false;  // whether delta application was succesfull or not
    }
    printClientMessage(sessionId_,"Server confirmed delta applied successfully");

    printClientMessage(sessionId_,"Content Pushed to the remote file");
    
    return true;
}

bool ClientSession::pullTransaction(const std::string& loaclPath,const std::string& remotePath){
    std::string command = "PULL\n";
    if (send(socketFD_, command.c_str(), command.size(), 0) != (ssize_t)command.size()) {
        printClientMessage(sessionId_,"Failed to send command");
        return false;
    }
    DataTransfer dataPipe; // used for data transfer

    if(!recievingStatus(dataPipe)) return false; // status for command part

    // sending the remotePath
    
    if(dataPipe.sendFilePath(socketFD_, remotePath)){
        printClientMessage(sessionId_,"Request sent to the remote machine");
    }else{
        printClientMessage(sessionId_,"Failed to send remote file path");
        return false;
    }

    if(!recievingStatus(dataPipe)) return false; // status for remote file path

    DestinationManager destination(loaclPath, "");  // using block size = 8
    // now this local machine will generate the hashes
    
    Result<std::vector<BlockInfo>> blockHashesResult= destination.getFileBlockHashes();
    if(!blockHashesResult.success){
        printClientMessage(sessionId_,"Error while generating block hashes:: "+blockHashesResult.message);
        return false;
    }
    // sending this block hashes
    printClientMessage(sessionId_,"Sending block hashes...");
    if(dataPipe.serializeAndSendBlockHashes(socketFD_,blockHashesResult.data)){
        printClientMessage(sessionId_,"Block Hashes sent successfully");
    }else{
        printClientMessage(sessionId_,"Failed to send block hashes");
        return false;
    }

    if(!recievingStatus(dataPipe)) return false; // status for whether block hashes are successfully recieved or not

    if(!recievingStatus(dataPipe)) return false; // status for whether delta calculated succesfully or not
    // recieve the delta instructions
    std::vector<DeltaInstruction> deltas;
    if(dataPipe.receiveDelta(socketFD_,deltas)){
        printClientMessage(sessionId_,"Delta Instructions recieved successfully");
    }else{
        printClientMessage(sessionId_,"Failed to get delta instructions");
        return false;
    }

    // now apply this delta
    Result<void> applyDetaRes= destination.applyDelta(deltas);
    if(applyDetaRes.success){
        printClientMessage(sessionId_,"Content pulled successfully");
    }else{
        printClientMessage(sessionId_,"Error while applying delta instructions");
    }
    return applyDetaRes.success;

}