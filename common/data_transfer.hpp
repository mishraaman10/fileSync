#pragma once
#include "block_info.hpp"
#include "delta_instruction.hpp"
#include<vector>
struct StatusMessage{
    bool status;
    std::string msg;
    StatusMessage(bool stat,std::string mess){
        status=stat;
        msg=mess;
    };
};

class DataTransfer{
public:
    bool serializeAndSendBlockHashes(const int socket, const std::vector<BlockInfo>& blockHashes);
    bool receiveBlockHashes(const int socketFD, std::vector<BlockInfo>& blocks);
    bool serializeAndSendDeltaInstructions(int socket, const std::vector<DeltaInstruction>& delta);
    bool receiveDelta(int socket, std::vector<DeltaInstruction>& delta);
    bool sendFilePath(int socketFD, const std::string& filePath);
    bool receiveFilePath(int socketFD, std::string& filePath);
    bool sendStatus(int socket,const StatusMessage& statusMessage);
    bool recieveStatus(int socket,StatusMessage &status);
private:
    bool sendAll(int socket, const void* buffer, size_t length);
    bool recvAll(int socketFD,void* buffer, size_t length);
};