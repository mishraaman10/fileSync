// Using it system will act as sevevr to provide the hashes of the destination files and then apply the result back

#pragma once
#include<atomic>
#include<mutex>
#include<condition_variable>

class ServerMode{
public:
    ServerMode(const int p);
    void startServer();
private:
    int port_;
    int serverSocket_;
    std::atomic<int> activeClients_;
    std::mutex mtx_;
    std::condition_variable cv_;
    void setupSocket();
    void acceptConnections();
    void handleClient(int clientSocket);
    bool pushTransaction(int clientSocket);
    bool pullTransaction(int clientSocket);
};