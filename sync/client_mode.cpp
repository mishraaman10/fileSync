#include "client_mode.hpp"
#include <iostream>
#include <sstream>
#include <thread>
#include <algorithm>

void ClientMode::startCLI() {
    std::cout << "Client CLI started. Type 'help' for commands.\n";
    std::string line;

    while (true) {
        std::cout << ">>> ";
        if (!std::getline(std::cin, line)) break;

        if (line.empty()) continue;

        handleCommand(line);
    }
}

void ClientMode::handleCommand(const std::string& cmd) {
    std::istringstream iss(cmd);
    std::string keyword;
    iss >> keyword;

    if (keyword == "help") {
        printHelp();
    } else if (keyword == "list") {
        listSessions();
    } else if (keyword == "connect") {
        std::string ip;
        int port;
        if (!(iss >> ip >> port)) {
            std::cerr << "Usage: connect <ip> <port>\n";
            return;
        }

        int idx = findFreeSlot();
        if (idx == -1) {
            std::cerr << "Max 3 connections allowed. Disconnect one first.\n";
            return;
        }

        auto session = std::make_unique<ClientSession>(ip, port, idx);
        if (!session->connectToServer()) {
            std::cerr << "Failed to connect.\n";
            return;
        }

        if (idx >= (int)sessions_.size())
            sessions_.resize(idx + 1);

        sessions_[idx] = std::move(session);

    }else if(keyword=="push"){
        int sessionId;
        std::string localPath,remotePath;
        if (!(iss >> sessionId >> localPath >> remotePath)) {
            std::cerr << "Usage: push <session_id> <local_path> <remote_path>\n";
            return;
        }
        if (sessionId < 0 || sessionId >= (int)sessions_.size() || !sessions_[sessionId]) {
            std::cerr << "Invalid session ID.\n";
            return;
        }
        std::thread([this, sessionId, localPath,remotePath]() {
            sessions_[sessionId]->runTransaction("push",localPath,remotePath);
        }).detach();

    }else if(keyword=="pull"){
        int sessionId;
        std::string localPath,remotePath;
        if (!(iss >> sessionId >> remotePath >> localPath)) {
            std::cerr << "Usage: push <session_id> <remote_path> <local_path>\n";
            return;
        }
        if (sessionId < 0 || sessionId >= (int)sessions_.size() || !sessions_[sessionId]) {
            std::cerr << "Invalid session ID.\n";
            return;
        }
        std::thread([this, sessionId, localPath,remotePath]() {
            sessions_[sessionId]->runTransaction("pull",localPath,remotePath);
        }).detach();
    }
     else if (keyword == "send") {
        int sessionId;
        std::string filepath;
        if (!(iss >> sessionId >> filepath)) {
            std::cerr << "Usage: send <session_id> <filepath>\n";
            return;
        }

        if (sessionId < 0 || sessionId >= (int)sessions_.size() || !sessions_[sessionId]) {
            std::cerr << "Invalid session ID.\n";
            return;
        }
    } else if (keyword == "disconnect") {
        int sessionId;
        if (!(iss >> sessionId)) {
            std::cerr << "Usage: disconnect <session_id>\n";
            return;
        }

        if (sessionId < 0 || sessionId >= (int)sessions_.size() || !sessions_[sessionId]) {
            std::cerr << "Invalid session ID.\n";
            return;
        }

        sessions_[sessionId]->close();
        sessions_[sessionId].reset();

    } else if (keyword == "exit") {
        std::cout << "Exiting...\n";
        for (auto& session : sessions_) {
            if (session) session->close();
        }
        exit(0);

    } else {
        std::cerr << "Unknown command. Type 'help' for available commands.\n";
    }
}


// Help for commands
void ClientMode::printHelp() const {
    std::cout << "Available Commands:\n"
              << " connect <ip> <port>                           Connect to server (max 3)\n"
              << " push <session_id> <local_path> <remote_path>  Push the local content to remote file/folder\n"
              << " pull <session_id> <remote_path> <local_path>  Pull the remote content to local file/folder\n"
              << " disconnect <session_id>                       Disconnect from server\n"
              << " list                                          List active sessions\n"
              << " help                                          Show this help\n"
              << " exit                                          Exit client\n";
}

void ClientMode::listSessions() const {
    std::cout << "Active Sessions:\n";
    for (size_t i = 0; i < sessions_.size(); ++i) {
        if (sessions_[i]&&sessions_[i]->isConnected()) {
            std::cout << "  [" << i << "] " << sessions_[i]->getInfo() << "\n";
        }
    }
}

int ClientMode::findFreeSlot() const {
    for (int i = 0; i < MAX_SESSIONS; ++i) {
        if (i >= (int)sessions_.size() || !sessions_[i]) {
            return i;
        }
    }
    return -1;
}