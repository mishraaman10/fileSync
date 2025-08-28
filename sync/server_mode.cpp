#include "server_mode.hpp"
#include "../common/thread_pool.hpp"
#include "../common/block_info.hpp"
#include "../destination/destination_manager.hpp"
#include "../source/source_manager.hpp"
#include "../common/data_transfer.hpp"
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <thread>
#include<fstream>
#include<vector>
const int MAX_CONNECTIONS = 3;

ServerMode::ServerMode(int port):port_(port),serverSocket_(-1),activeClients_(0){}

void ServerMode::startServer(){
    // will start the server and listen for the incoming request 
    setupSocket();
    acceptConnections();
}

void ServerMode::setupSocket() {
    serverSocket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket_ < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    if (setsockopt(serverSocket_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("Set socket options failed");
        exit(EXIT_FAILURE);
    }

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(port_);

    if (bind(serverSocket_, (sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(serverSocket_, MAX_CONNECTIONS) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    std::cout << "[Server] Listening on port " << port_ << "...\n";
}

void ServerMode::acceptConnections() {
    ThreadPool pool(MAX_CONNECTIONS);

    while (true) {
        std::unique_lock<std::mutex> lock(mtx_);
        cv_.wait(lock,[this](){
            return activeClients_<3;
        });
        sockaddr_in clientAddr{};
        socklen_t addrLen = sizeof(clientAddr);
        int clientSocket = accept(serverSocket_, (sockaddr*)&clientAddr, &addrLen);
        if (clientSocket < 0) {
            perror("Accept failed");
            continue;
        }

        activeClients_++;  // increment here under lock
        lock.unlock();

        char clientIP[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &clientAddr.sin_addr, clientIP, INET_ADDRSTRLEN);
        std::cout << "[Server] Connection accepted from " << clientIP << "\n";

        //  thread pool to handle client
        pool.submit([this, clientSocket]() {
            this->handleClient(clientSocket);

            // this client is done now give someone other chance
            {
                std::lock_guard<std::mutex> guard(mtx_);
                activeClients_--;
            }
            cv_.notify_one();
        });
    }
}

// handleClient for communication with the client 

// after evey logical step server need to send the status message to the client
// whether successfull or not
void ServerMode::handleClient(int clientSocket){
    char cmd[5]; // For 4-char mode + '\n'
    ssize_t bytesRead = recv(clientSocket, cmd, sizeof(cmd), MSG_WAITALL);
    DataTransfer pipe;
    if (bytesRead != 5|| cmd[4]!='\n' ) {
        pipe.sendStatus(clientSocket,StatusMessage(false,"Failed to read 5-byte command"));
        close(clientSocket);
        return;
    }
    

    std::string mode(cmd, 4); // First 4 chars are the command
    pipe.sendStatus(clientSocket,StatusMessage(true,"Starting the "+mode+" request"));
    if(mode=="PUSH"){
        pushTransaction(clientSocket);
    }else if(mode=="PULL"){
        pullTransaction(clientSocket);
    }else{
        std::cerr << "Invalid mode\n";
    }
    close(clientSocket);
}

bool ServerMode::pushTransaction(int clientSocket) {
    std::string remotePath;
    std::string sourceFileName;

    DataTransfer dataPipe;
    // 1. Receive remote file path
    if (dataPipe.receiveFilePath(clientSocket,remotePath)){ 
        
        dataPipe.sendStatus(clientSocket ,StatusMessage(true,"Genrating the block hashes for remote file..."));
    }else{
        
        dataPipe.sendStatus(clientSocket ,StatusMessage(false,"Error while recieving remote path"));
        return false;
    }

    // 2. Receive source file name
    if (!dataPipe.receiveFilePath(clientSocket, sourceFileName)) {
        std::cout << "Failed to receive source file name" << std::endl;
        dataPipe.sendStatus(clientSocket, StatusMessage(false, "Error while receiving source file name"));
        return false;
    }
    std::cout << "Received sourceFileName: '" << sourceFileName << "'" << std::endl;

    // 3. Get existing file block hashes
    std::cout << "Creating DestinationManager with remotePath: '" << remotePath << "', sourceFileName: '" << sourceFileName << "'" << std::endl;
    DestinationManager dest(remotePath, sourceFileName);  // Block size = 8
    Result<std::vector<BlockInfo>> blockHashesResult = dest.getFileBlockHashes();

    
    

    if(!blockHashesResult.success){
        
        dataPipe.sendStatus(clientSocket ,StatusMessage(false,"Error while genrating block hashes for remote file:: "+blockHashesResult.message));
        return false;
    }else{
        dataPipe.sendStatus(clientSocket ,StatusMessage(true,"Block hashes for remote generated successfully, Sending it over the channel"));
    }

    // 4. Send the block hashes to the client
    if (!dataPipe.serializeAndSendBlockHashes(clientSocket, blockHashesResult.data)) {
        return false;
    }

    // 5. Receive delta from client
    
    std::vector<DeltaInstruction> delta;
    if (dataPipe.receiveDelta(clientSocket, delta)) {
        
        dataPipe.sendStatus(clientSocket ,StatusMessage(true,"Recieved delta instructions successfully"));
    }else{
        std::cout << "Failed to receive delta instructions" << std::endl;
        dataPipe.sendStatus(clientSocket ,StatusMessage(false,"Error while recieving delta messages"));
        return false;
    }

    // 6. Apply delta to remote file
    Result<void> applyDetaRes= dest.applyDelta(delta);
    if(applyDetaRes.success){
        dataPipe.sendStatus(clientSocket ,StatusMessage(true,"Push request performed successfully"));
    }else{
        dataPipe.sendStatus(clientSocket ,StatusMessage(false,"Error while applying delta:: " + applyDetaRes.message));
        return false;
    }

    return true;
}

bool ServerMode::pullTransaction(int clientSocket){
    std::string remotePath;

    DataTransfer dataPipe;
    // 1. Receive remote file path
    if (dataPipe.receiveFilePath(clientSocket,remotePath)){ 
        dataPipe.sendStatus(clientSocket ,StatusMessage(true,"Recieved the remote file path"));
    }else{
        dataPipe.sendStatus(clientSocket ,StatusMessage(false,"Error while recieving remote path"));
        return false;
    }

    // recieve the block hashes
    std::vector<BlockInfo> blockHashes;
    if(dataPipe.receiveBlockHashes(clientSocket,blockHashes)){
        dataPipe.sendStatus(clientSocket ,StatusMessage(true,"Block hashes recieved successfully, Generating delta instructions..."));
    }else{
        dataPipe.sendStatus(clientSocket ,StatusMessage(false,"Error while recieving block hashes"));
        return false;
    }

    // generate deltas
    SourceManager source(remotePath,blockHashes);
    Result<std::vector<DeltaInstruction>> deltaResult=source.getDelta();
    if(deltaResult.success){
        dataPipe.sendStatus(clientSocket ,StatusMessage(true,"Delta instructions generated successfully, Sending it over the channel"));
    }else{
        dataPipe.sendStatus(clientSocket ,StatusMessage(false,"Error while generating delta:: " + deltaResult.message));
        return false;
    }
    // send this deltas
    if(!dataPipe.serializeAndSendDeltaInstructions(clientSocket,deltaResult.data)){
        return false;
    }

    return true;

}