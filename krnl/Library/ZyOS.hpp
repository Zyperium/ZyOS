#pragma once
#include <stdint.h>

namespace ZyOS {
    using WORD = uint16_t;
    using DWORD = uint32_t;
    using QWORD = uint64_t;

    constexpr uint64_t sbWORD = 16;
    constexpr uint64_t sbDWORD = 32;
    constexpr uint64_t sbQWORD = 64;

    constexpr uint64_t END_OF_LOWER_HALF = 0x00007FFFFFFFFFFF;
    constexpr uint64_t START_OF_UPPER_HALF = 0xFFFF800000000000;
}