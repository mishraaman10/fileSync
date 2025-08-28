#pragma once
#include <string>
#include <cstdint>

struct BlockInfo {
    size_t offset;              // Offset in destination file
    uint32_t weakHash;          // Rolling hash
    std::string strongHash;     // SHA-1 or MD5

    BlockInfo(size_t o, uint32_t w, const std::string& s)
        : offset(o), weakHash(w), strongHash(s) {}
};
