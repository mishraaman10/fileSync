#include "source_manager.hpp"
#include "../common/hash_utils.hpp"
#include "../common/thread_pool.hpp"
#include "../common/config.hpp"
#include <fstream>
#include<iostream>
#include<vector>
#include<deque>


SourceManager::SourceManager(const std::string& sourcePath,const std::vector<BlockInfo>& destBlocks) : sourcePath_(sourcePath),destBlocks_(std::move(destBlocks)),blockSize_(Config::BLOCK_SIZE),chunkSize_(Config::CHUNK_SIZE){
    for(auto blockInfo:destBlocks_){
        destHashToOffset[{blockInfo.weakHash,blockInfo.strongHash}]=blockInfo.offset;
        weakHashSet.insert(blockInfo.weakHash);
    }
}

// Processing in chunks
// it is confirm that chunkSize>blockSize_
// chunk is to be very large
void SourceManager::ProcessChunk(size_t start,size_t chunkId,std::vector<std::vector<DeltaInstruction>> &result) const{
    std::ifstream file(sourcePath_, std::ios::binary);
    if (!file) {
        std::cerr << "Error opening file: " << sourcePath_ << '\n';
        return;
    }

    file.seekg(0, std::ios::end);         // move to end
    size_t fileSize = file.tellg();       // get position = size
    file.seekg(start, std::ios::beg);     // seek back to start for reading

    if (start >= fileSize) {
        std::cerr << "Start offset " << start << " is beyond file size for chunk " << chunkId<< " File size"<<fileSize << '\n';
    }

    std::vector<DeltaInstruction> deltas;  // to store the delta instructions

    std::vector<char> buffer(blockSize_);
    std::vector<char> pendingInsert;  // to be inserted when no match has been found

    size_t offset = start,startIndex=0;   // startIndex is for correct order in buffer due to circular shift
    size_t totalBytesRead=0; // will be helpful in keeping track so that we do not step in the next chunk
    file.read(buffer.data(), blockSize_);

    size_t bytesInWindow = file.gcount();
    totalBytesRead+=bytesInWindow;
    if (bytesInWindow == 0) return ;

    uint32_t hash = HashUtils::computeWeakHash(buffer.data(), bytesInWindow);

    const uint64_t base = 257;
    const uint64_t mod = 1e9 + 7;
    uint64_t basePow = 1;
    for (size_t i = 1; i < blockSize_; ++i)
        basePow = (basePow * base) % mod;

    while(true){
        if(bytesInWindow<blockSize_){
            // just insert that complete window pendingInsert and that will be at the end of file
            for(size_t ind=0;ind<bytesInWindow;ind++) pendingInsert.push_back(buffer[ind]);
            break;
        }

        bool matched=false;
        auto it=weakHashSet.find(hash);
        if(it!=weakHashSet.end()){
            // weak hash matches something lets confirm with strong hash
            std::vector<char> snapshot(blockSize_);
            for (size_t i = 0; i < blockSize_; ++i) {
                snapshot[i] = buffer[(startIndex + i) % blockSize_];
            }
            std::string strongHashForWindow=HashUtils::computeStrongHash(snapshot.data(),blockSize_);
            auto ptr=destHashToOffset.find({hash,strongHashForWindow});
            if(ptr!=destHashToOffset.end()){
                // exact match found
                matched=true;
                if (!pendingInsert.empty()) {
                    // these characters are not matched and need to be inserted
                    deltas.push_back(DeltaInstruction::makeInsert(pendingInsert));
                    pendingInsert.clear();
                }
                deltas.push_back(DeltaInstruction::makeCopy(ptr->second));
            }
        }

        if(matched){
            // then need to skip the offset by window size
            offset += blockSize_;
            file.clear();
            file.seekg(offset);
            size_t bytesToRead=std::min(blockSize_,chunkSize_-totalBytesRead);  // if going outside chunk then don't read it
            file.read(buffer.data(), bytesToRead);
            bytesInWindow = file.gcount();
            totalBytesRead+=bytesInWindow;
            if (bytesInWindow==0) break;
            startIndex=0;
            hash = HashUtils::computeWeakHash(buffer.data(), bytesInWindow);
        }else{
            // emit 1 byte insert
            pendingInsert.push_back(buffer[startIndex]);

            // Slide window
            char outByte = buffer[startIndex];
            char inByte;
            file.read(&inByte, 1);
            totalBytesRead++;
            if (!file||totalBytesRead>chunkSize_) {
                // all remaining thing to be inserted in pending 
                int tempInd=(startIndex+1)%blockSize_;
                while(tempInd!=startIndex){
                    pendingInsert.push_back(buffer[tempInd]);
                    tempInd=(tempInd+1)%blockSize_;
                }
                break;
            }
            offset += 1;
            buffer[startIndex] = inByte;
            startIndex = (startIndex + 1) % blockSize_;
            
            hash = (mod + hash - (static_cast<uint64_t>(outByte) * basePow) % mod) % mod;
            hash = (hash * base + static_cast<unsigned char>(inByte)) % mod;
        }
    }

    if (!pendingInsert.empty())
        deltas.push_back(DeltaInstruction::makeInsert(pendingInsert));
    result[chunkId]=std::move(deltas);

}


Result<std::vector<DeltaInstruction>> SourceManager::getDelta() const{
    try{
        const size_t THREAD_COUNT = 4;

        std::ifstream file(sourcePath_, std::ios::binary | std::ios::ate);
        if (!file) {
            return Result<std::vector<DeltaInstruction>>::Error("Failed to open source file");
        }
        size_t fileSize = file.tellg();
        
        size_t totalChunks = fileSize/chunkSize_;
        if(fileSize%chunkSize_) totalChunks++;  // last chunk not complete

        std::vector<std::vector<DeltaInstruction>> chunksResult(totalChunks);

        ThreadPool pool(THREAD_COUNT);
        std::vector<std::future<void>> futures;

        for (size_t chunkId = 0; chunkId < totalChunks; ++chunkId) {
            size_t start = chunkId * chunkSize_;

            futures.emplace_back(
                pool.submit([=,&chunksResult]() {
                    this->ProcessChunk(start, chunkId, chunksResult);
                })
            );
        }

        // Wait for all threads to complete
        for (auto& f : futures) {
            f.get();
        }

        std::vector<DeltaInstruction> combinedResult;
        for (const auto& chunk : chunksResult) {
            combinedResult.insert(combinedResult.end(), chunk.begin(), chunk.end());
        }
        
        return Result<std::vector<DeltaInstruction>>::Ok(combinedResult);

    }catch(const std::exception &e){
        return Result<std::vector<DeltaInstruction>>::Error(std::string("Exception in getDelta: ") + e.what());
    }catch(...){
        return Result<std::vector<DeltaInstruction>>::Error("Unknown error occurred in getDelta()");
    }
    

}