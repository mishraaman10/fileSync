#pragma once
#include "client_session.hpp"
#include <memory>
#include <vector>
#include <string>

class ClientMode {
public:
    void startCLI();

private:
    static const int MAX_SESSIONS = 3;
    std::vector<std::unique_ptr<ClientSession>> sessions_;

    void handleCommand(const std::string& cmd);
    void printHelp() const;
    void listSessions() const;
    int findFreeSlot() const;
};
