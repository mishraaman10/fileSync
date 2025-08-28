#include "sync_engine.hpp"
#include "../destination/destination_manager.hpp"
#include "../source/source_manager.hpp"
#include<iostream>
SyncEngine:: SyncEngine(const std::string& sourcePath, const std::string& destPath, size_t blockSize):sourcePath_(sourcePath),destPath_(destPath),blockSize_(blockSize){}

void SyncEngine::syncFile(){
    // Extract filename from source path for destination
    std::string sourceFileName = sourcePath_;
    size_t pos = sourceFileName.find_last_of("/\\");
    if (pos != std::string::npos) {
        sourceFileName = sourceFileName.substr(pos + 1);
    }
    
    DestinationManager dest(destPath_, sourceFileName);
    auto blocks = dest.getFileBlockHashes().data;
    SourceManager src(sourcePath_,blocks);
    auto deltaData=src.getDelta().data;
    dest.applyDelta(deltaData);
    std::cout<<"Sync Completed\n";
}