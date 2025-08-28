#pragma once
#include<string>
#include<vector>
#include <unordered_map>
#include<unordered_set>
#include<utility>
#include "../common/block_info.hpp"
#include "../common/delta_instruction.hpp"
#include "../common/result.hpp"

struct PairHash {
    template <class T1, class T2>
    std::size_t operator () (const std::pair<T1, T2>& p) const {
        auto h1 = std::hash<T1>{}(p.first);
        auto h2 = std::hash<T2>{}(p.second);

        // A common way to combine hashes (boost::hash_combine equivalent)
        // You can experiment with other combinations if needed, but this is a good starting point.
        return h1 ^ (h2 << 1);
    }
};


class SourceManager{
public:
    SourceManager(const std::string& sourcePath,const std::vector<BlockInfo>& destBlocks);
    Result<std::vector<DeltaInstruction>> getDelta() const;

private:
    void ProcessChunk(size_t start,size_t chunkId,std::vector<std::vector<DeltaInstruction>> &result) const;
    std::string sourcePath_;
    std::vector<BlockInfo> destBlocks_;
    size_t blockSize_;
    size_t chunkSize_;
    std::unordered_map<std::pair<uint32_t,std::string>,size_t,PairHash> destHashToOffset;
    std::unordered_set<uint32_t> weakHashSet;
};