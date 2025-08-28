// config.hpp
#pragma once

namespace Config {
    inline constexpr int BLOCK_SIZE = 128;     // block size 128 kb
    inline constexpr int CHUNK_SIZE=128*1024;  // processing 128 mb chunks for parallel processing
}
