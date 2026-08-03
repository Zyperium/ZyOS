#pragma once
#include <stdint.h>

namespace Decomp {
    constexpr uint64_t LINE_SIZE = 64;

    struct DisasmLine {
        uint64_t addr{0};
        bool is_rip{0};
        char text[LINE_SIZE]{0};
    };

    struct Disasmrep {
        int total_lines{0};
        DisasmLine *lines{nullptr};
        int count{0};

        ~Disasmrep();
    };

    // Make sure to log lines->text (I may forget this)
    Disasmrep *decomp_and_log(uint64_t at_addr, uint8_t around);
}