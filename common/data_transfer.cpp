#include "data_transfer.hpp"
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include<iostream>


bool DataTransfer::sendAll(int socket, const void* buffer, size_t length) {
    const char* data = static_cast<const char*>(buffer);
    while (length > 0) {
        ssize_t sent = send(socket, data, length, 0);
        if (sent <= 0) return false;
        data += sent;
        length -= sent;
    }
    return true;
}

bool DataTransfer::serializeAndSendBlockHashes(const int socket,const std::vector<BlockInfo>& blockHashes){
    uint32_t count = htonl(blockHashes.size());
    if (!sendAll(socket, &count, sizeof(count))) {
        return false;
    }

    for (size_t i = 0; i < blockHashes.size(); ++i) {
        const BlockInfo& b = blockHashes[i];
        uint64_t offset_net = htobe64(b.offset);
        uint32_t weakHash_net = htonl(b.weakHash);
        uint32_t strongLen = htonl(static_cast<uint32_t>(b.strongHash.size()));

        if (!sendAll(socket, &offset_net, sizeof(offset_net)) ||
            !sendAll(socket, &weakHash_net, sizeof(weakHash_net)) ||
            !sendAll(socket, &strongLen, sizeof(strongLen)) ||
            !sendAll(socket, b.strongHash.data(), b.strongHash.size())) {
            return false;
        }
    }
    return true;
}

bool DataTransfer::recvAll(int socketFD, void* buffer, size_t length) {
    char* data = static_cast<char*>(buffer);
    while (length > 0) {
        ssize_t received = recv(socketFD, data, length, MSG_WAITALL);
        if (received <= 0) return false;
        data += received;
        length -= received;
    }
    return true;
}

bool  DataTransfer::receiveBlockHashes(int socketFD, std::vector<BlockInfo>& blocks) {
    uint32_t blockCountNet;
    if (!recvAll(socketFD, &blockCountNet, sizeof(blockCountNet))) {
        return false;
    }

    uint32_t blockCount = ntohl(blockCountNet);
    blocks.reserve(blockCount);

    for (uint32_t i = 0; i < blockCount; ++i) {
        uint64_t offsetNet;
        uint32_t weakHashNet, strongLenNet;

        if (!recvAll(socketFD, &offsetNet, sizeof(offsetNet))) {
            return false;
        }
        if (!recvAll(socketFD, &weakHashNet, sizeof(weakHashNet))) {
            return false;
        }
        if (!recvAll(socketFD, &strongLenNet, sizeof(strongLenNet))) {
            return false;
        }

        uint64_t offset = be64toh(offsetNet);
        uint32_t weakHash = ntohl(weakHashNet);
        uint32_t strongLen = ntohl(strongLenNet);

        std::vector<char> hashBuf(strongLen);
        if (!recvAll(socketFD, hashBuf.data(), strongLen)) {
            return false;
        }

        blocks.emplace_back(offset, weakHash, std::string(hashBuf.begin(), hashBuf.end()));
    }

    return true;
}

bool DataTransfer::serializeAndSendDeltaInstructions(int socket, const std::vector<DeltaInstruction>& delta) {
    uint32_t count = htonl(delta.size());
    if (!sendAll(socket, &count, sizeof(count))) {
        return false;
    }

    for (size_t i = 0; i < delta.size(); ++i) {
        const DeltaInstruction& inst = delta[i];
        uint8_t type = static_cast<uint8_t>(inst.type);
        if (!sendAll(socket, &type, sizeof(type))) {
            return false;
        }

        if (inst.type == DeltaType::COPY) {
            uint64_t offset_net = htobe64(inst.offset);
            if (!sendAll(socket, &offset_net, sizeof(offset_net))) {
                return false;
            }

        } else if (inst.type == DeltaType::INSERT) {
            uint32_t dataLen = htonl(inst.data.size());
            if (!sendAll(socket, &dataLen, sizeof(dataLen))) {
                return false;
            }

            if (!inst.data.empty() && !sendAll(socket, inst.data.data(), inst.data.size())) {
                return false;
            }
        } else {
            return false;
        }
    }

    return true;
}

bool DataTransfer::receiveDelta(int socket, std::vector<DeltaInstruction>& delta) {
    delta.clear();
    

    // 1. Receive count of delta instructions
    uint32_t countNet;
    if (!recvAll(socket, &countNet, sizeof(countNet))) {
        std::cout << "  Failed to receive delta count" << std::endl;
        return false;
    }
    uint32_t count = ntohl(countNet);
    

    for (uint32_t i = 0; i < count; ++i) {
        
        // 2. Receive type
        uint8_t typeByte;
        if (!recvAll(socket, &typeByte, sizeof(typeByte))) {
            std::cout << "  Failed to receive type for instruction " << i << std::endl;
            return false;
        }
        DeltaType type = static_cast<DeltaType>(typeByte);
        

        if (type == DeltaType::COPY) {
            // 3. Receive offset for COPY
            uint64_t offsetNet;
            if (!recvAll(socket, &offsetNet, sizeof(offsetNet))) return false;
            size_t offset = be64toh(offsetNet);

            delta.push_back(DeltaInstruction::makeCopy(offset));
        } else if (type == DeltaType::INSERT) {
            // 4. Receive data length
            uint32_t dataLenNet;
            if (!recvAll(socket, &dataLenNet, sizeof(dataLenNet))) return false;
            uint32_t dataLen = ntohl(dataLenNet);
            

            // 5. Receive data
            std::vector<char> data(dataLen);
            if (dataLen > 0 && !recvAll(socket, data.data(), dataLen)) return false;
            
            
            
            std::cout << std::dec << std::endl;

            delta.push_back(DeltaInstruction::makeInsert(data));
        } else {
            std::cerr << "[Error] Unknown DeltaType received\n";
            return false;
        }
    }

    return true;
}

bool DataTransfer::sendFilePath(int socketFD, const std::string& filePath) {
    uint32_t pathLen = htonl(filePath.size());

    if (!sendAll(socketFD, &pathLen, sizeof(pathLen))) {
        std::cerr << "Failed to send path length\n";
        return false;
    }

    if (!sendAll(socketFD, filePath.c_str(), filePath.size())) {
        std::cerr << "Failed to send path string\n";
        return false;
    }

    return true;
}

bool DataTransfer::receiveFilePath(int socketFD, std::string& filePath) {
    uint32_t pathLenNet = 0;

    if (!recvAll(socketFD, &pathLenNet, sizeof(pathLenNet))) {
        std::cerr << "Failed to receive path length\n";
        return false;
    }

    uint32_t pathLen = ntohl(pathLenNet);
    filePath.resize(pathLen);

    if (!recvAll(socketFD, &filePath[0], pathLen)) {
        std::cerr << "Failed to receive path string\n";
        return false;
    }

    return true;
}


bool DataTransfer::sendStatus(int socketFD, const StatusMessage& statusMsg) {
    uint8_t statusByte = statusMsg.status ? 1 : 0;
    uint32_t msgLen = htonl(statusMsg.msg.size());

    // Send status (1 byte)
    if (!sendAll(socketFD, &statusByte, sizeof(statusByte))) {
        std::cerr << "Failed to send status byte\n";
        return false;
    }

    if (!sendAll(socketFD, &msgLen, sizeof(msgLen))) {
        std::cerr << "Failed to send message length\n";
        return false;
    }

    // Send message string (msgLen bytes)
    if (!sendAll(socketFD, statusMsg.msg.c_str(), statusMsg.msg.size())) {
        std::cerr << "Failed to send message string\n";
        return false;
    }

    return true;
}

bool DataTransfer::recieveStatus(int socket,StatusMessage& statusMessage){
    uint8_t statusByte = 0;

    // Receive the 1-byte status
    if (!recvAll(socket, &statusByte, sizeof(statusByte))) {
        std::cerr << "Failed to receive status byte\n";
        return false;
    }

    uint32_t msgLenNet = 0;

    // Receive the 4-byte message length
    if (!recvAll(socket, &msgLenNet, sizeof(msgLenNet))) {
        std::cerr << "Failed to receive message length\n";
        return false;
    }

    uint32_t msgLen = ntohl(msgLenNet);
    std::string msg;
    msg.resize(msgLen);

    if (!recvAll(socket, &msg[0], msgLen)) {
        std::cerr << "Failed to receive message string\n";
        return false;
    }

    statusMessage = StatusMessage(statusByte != 0, std::move(msg));
    return true;
}