#include "destination_manager.hpp"
#include <fstream>
#include<iostream>
#include "../common/hash_utils.hpp"
#include "../common/config.hpp"
#include<stdexcept>
// Helper to check if path is a directory
#include <filesystem>

DestinationManager::DestinationManager(const std::string& destinationPath, const std::string& sourceFileName) : blockSize_(Config::BLOCK_SIZE) {
    namespace fs = std::filesystem;
    if (fs::is_directory(destinationPath)) {
        // Append the source filename if destination is a directory
        destPath_ = (fs::path(destinationPath) / fs::path(sourceFileName)).string();
    } else {
        destPath_ = destinationPath;
    }
}

Result<std::vector<BlockInfo>> DestinationManager::getFileBlockHashes() const{
    std::ifstream file(destPath_, std::ios::binary);
    std::vector<BlockInfo> blocks;

    if (!file) {
        // File doesn't exist - this is normal for new files being created
        std::cout << "Destination file doesn't exist (Creating ....): " << destPath_ << std::endl;
        return Result<std::vector<BlockInfo>>::Ok(blocks); // Return empty block list for new files
    }

    size_t offset = 0;
    std::vector<char> buffer(blockSize_);

    try{
        while (file.read(buffer.data(), blockSize_) || file.gcount() > 0) {
            size_t bytesRead = file.gcount();
            uint32_t weakHash = HashUtils::computeWeakHash(buffer.data(), bytesRead);
            std::string strongHash = HashUtils::computeStrongHash(buffer.data(),bytesRead);
            blocks.emplace_back(offset, weakHash, strongHash);
            offset += bytesRead;
        }
    }catch(const std::exception& e){
        return Result<std::vector<BlockInfo>>::Error(std::string("Error while hashing: ") + e.what());
    }catch(...){
        return Result<std::vector<BlockInfo>>::Error("Unknown error occurred while hashing file blocks.");
    }

    // return the result with ok and blocks
    return Result<std::vector<BlockInfo>>::Ok(blocks);
}

Result<void> DestinationManager::applyDelta(const std::vector<DeltaInstruction>& deltas){
    namespace fs = std::filesystem;
    
    // Ensure the destination directory exists
    fs::path destFilePath(destPath_);
    if (destFilePath.has_parent_path()) {
        std::error_code ec;
        fs::create_directories(destFilePath.parent_path(), ec);
        if (ec) {
            return Result<void>::Error("Failed to create directory structure: " + ec.message());
        }
    }
    
    std::ifstream oldFile(destPath_, std::ios::binary);
    std::string tempFilePath = destPath_ + ".sync.tmp";
    std::ofstream tempFile(tempFilePath, std::ios::binary | std::ios::trunc);

    // For new files, oldFile won't open, but that's okay
    bool isNewFile = !oldFile.is_open();
    if (!tempFile.is_open()) {
        return Result<void>::Error("Failed to create temporary file for delta application");
    }

    try{
        for (const auto& delta : deltas) {
            if (delta.type == DeltaType::COPY) {
                if (!isNewFile) {
                    oldFile.seekg(delta.offset, std::ios::beg);
                    std::vector<char> buffer(blockSize_);
                    oldFile.read(buffer.data(), blockSize_);
                    tempFile.write(buffer.data(), oldFile.gcount());
                }
                // For new files, COPY operations are ignored (no existing content to copy)
            } else if (delta.type == DeltaType::INSERT) {
                
                
                
                tempFile.write(delta.data.data(), delta.data.size());
                
            }
        }

        if (!isNewFile) {
            oldFile.close();
        }
        tempFile.close();

        // Atomic swap
        if (!isNewFile) {
            std::remove(destPath_.c_str());                // Delete old file
        }
        std::rename(tempFilePath.c_str(), destPath_.c_str()); // Replace with new
        
        std::cout << "File successfully " << (isNewFile ? "created" : "updated") << ": " << destPath_ << std::endl;
    }catch(const std::exception &e){
        return Result<void>::Error(std::string("Exception during delta apply: ") + e.what());
    }catch(...){
        return Result<void>::Error("Unknown error during delta apply");
    }

    return Result<void>::Ok();
}