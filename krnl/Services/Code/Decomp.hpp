#pragma once
#include <stdint.h>

namespace Decomp {
    void decomp_and_log(uint64_t at_addr, uint8_t around);

    constexpr uint64_t LINE_SIZE = 64;
}