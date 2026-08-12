#pragma once
#include <stdint.h>
#include <stddef.h>

namespace HAL::MEM::FMEM {
    void FastFill8(uint8_t* dest, uint8_t val, size_t count);
    void FastFill32(uint32_t* dest, uint32_t color, size_t count);
    void FastCopy(void* dest, const void* src, size_t bytes);
    void enable_sse();

    constexpr uint64_t CR4_OSFXSR       =   (1ULL << 9);
    constexpr uint64_t CR4_OSXMMEXCPT   =   (1ULL << 10);
    constexpr uint64_t CR4_OSXSAVE      =   (1ULL << 18);
    constexpr uint64_t XCR0_X87         =   (1ULL << 0);
    constexpr uint64_t XCR0_SSE         =   (1ULL << 1);
    constexpr uint64_t XCR0_AVX         =   (1ULL << 2);
    constexpr uint64_t XCR0_OPMASK      =   (1ULL << 5);
    constexpr uint64_t XCR0_ZMM_HI256   =   (1ULL << 6);
    constexpr uint64_t XCR0_HI16_ZMM    =   (1ULL << 7);
}
