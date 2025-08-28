#pragma once
#include <cstdint>
#include <string>
#include <openssl/sha.h>
class HashUtils {
public:

    static uint32_t computeWeakHash(const char* data, size_t len) {
        const uint64_t base = 257;
        const uint64_t mod = 1000000007;

        uint64_t hash = 0;
        for (size_t i = 0; i < len; ++i) {
            hash = (hash * base + static_cast<unsigned char>(data[i])) % mod;
        }
        return static_cast<uint32_t>(hash);
    }

    static std::string computeStrongHash(const char* data, size_t len) {
        unsigned char hash[SHA_DIGEST_LENGTH];
        SHA1(reinterpret_cast<const unsigned char*>(data), len, hash);

        static const char* hex = "0123456789abcdef";
        std::string result;
        for (int i = 0; i < SHA_DIGEST_LENGTH; ++i) {
            result += hex[(hash[i] >> 4) & 0xF];
            result += hex[hash[i] & 0xF];
        }
        return result;
    }

};
