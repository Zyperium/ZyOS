#pragma once
#include <stdint.h>
#include <stddef.h>

namespace FMEM {
    void FastFill8(uint8_t* dest, uint8_t val, size_t count);
    void FastFill32(uint32_t* dest, uint32_t color, size_t count);
    void FastCopy(void* dest, const void* src, size_t bytes);
    bool ValidateUserBuffer(uint64_t start_addr, size_t size);
    void enable_sse();
}
