#pragma once
#include<string>

class SyncEngine {
public:
    SyncEngine(const std::string& sourcePath, const std::string& destPath, size_t blockSize = 4096);
    void syncFile(); // calls DestinationManager + SourceDeltaGenerator + apply
private:
    std::string sourcePath_;
    std::string destPath_;
    size_t blockSize_;
};
