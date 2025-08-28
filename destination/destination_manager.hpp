#pragma once
#include<string>
#include<vector>
#include "../common/block_info.hpp"
#include "../common/delta_instruction.hpp"
#include "../common/result.hpp"
class DestinationManager{
public:
    DestinationManager(const std::string& destinationPath, const std::string& sourceFileName);
    Result<std::vector<BlockInfo>> getFileBlockHashes() const;
    Result<void> applyDelta(const std::vector<DeltaInstruction>& deltas);
private:
    std::string destPath_;
    size_t blockSize_;
};