#pragma once
#include <vector>
#include <string>

enum class DeltaType { COPY, INSERT };

struct DeltaInstruction {
    DeltaType type;
    size_t offset; // used for COPY
    std::vector<char> data; // used for INSERT

    static DeltaInstruction makeCopy(size_t offset) {
        return { DeltaType::COPY, offset, {} };
    }

    static DeltaInstruction makeInsert(const std::vector<char>& data) {
        return { DeltaType::INSERT, 0, data };
    }
};
