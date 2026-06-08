#pragma once
#include <stdint.h>

namespace ZyOS {
    using WORD = uint16_t;
    using DWORD = uint32_t;
    using QWORD = uint64_t;

    constexpr uint64_t sbWORD = 16;
    constexpr uint64_t sbDWORD = 32;
    constexpr uint64_t sbQWORD = 64;
}