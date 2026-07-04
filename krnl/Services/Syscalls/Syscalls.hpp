#pragma once
#include <stdint.h>

extern "C" void SysEntry();

namespace Syscalls {
    struct REGS {
        uint64_t ID;
        uint64_t A1;
        uint64_t A2;
        uint64_t A3;
        uint64_t A4;
        uint64_t A5;
    };

    struct SUBREGS {
        uint64_t A1;
        uint64_t A2;
        uint64_t A3;
        uint64_t A4;
        uint64_t A5;
    };

    enum class SYSCALL_ID : uint64_t {
        SYS_OPEN,
        SYS_READ,
        SYS_WRITE,
        SYS_CLOSE,
        MAX_ID
    };

    void initialize();

    constexpr uint64_t MSR_STAR_VAL = 0x00130008ULL << 32;
    constexpr uint64_t MSR_FSTAR_VAL = 0x202; // clean RFLAGS
}