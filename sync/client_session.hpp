#pragma once
#include "../common/block_info.hpp"
#include "../common/data_transfer.hpp"

#include<string>
#include<vector>
class ClientSession {
public:
    ClientSession(const std::string& ip, int port, int id);
    ~ClientSession();
    
    bool connectToServer();
    void close();
    bool isConnected() const;

    void runTransaction(const std::string request, const std::string& localPath,const std::string&remotePath); // one interaction
    std::string getInfo() const;

private:
    std::string ip_;
    int port_;
    int sessionId_;
    int socketFD_;
    bool connected_;

    bool pullTransaction(const std::string& loaclPath,const std::string& remotePath);
    bool pushTransaction(const std::string& loaclPath,const std::string& remotePath);
    bool recievingStatus(DataTransfer &dataPipe);
};
