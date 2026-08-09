#pragma once
#include <stddef.h>
#include <stdint.h>

#include <Library/cystr.hpp>

namespace lib {
    inline uint64_t fnv1a_hash(const void* key, size_t len) {
        const uint8_t* bytes = static_cast<const uint8_t*>(key);
        uint64_t hash = 14695981039346656037ULL;
        for (size_t i = 0; i < len; ++i) {
            hash ^= bytes[i];
            hash *= 1099511628211ULL;
        }
        return hash;
    }

    template <typename K>
    struct Hash {
        uint64_t operator()(const K &key) const {
            return fnv1a_hash(&key, sizeof(K));
        }
    };

    template <>
    struct Hash<lib::string> {
        uint64_t operator()(const lib::string& str) const {
            return fnv1a_hash(str.c_str(), str.length());
        }
    };
}