#pragma once
#include <stdint.h>

extern "C" void SysEntry();

namespace Syscalls {
    struct REGS {
        // RAX
        uint64_t ID;
        // RDI
        uint64_t A1;
        // RSI
        uint64_t A2;
        // RDX
        uint64_t A3;
        // R10
        uint64_t A4;
        // R8
        uint64_t A5;
    };

    struct SUBREGS {
        // RDI
        uint64_t A1;
        // RSI
        uint64_t A2;
        // RDX
        uint64_t A3;
        // R10
        uint64_t A4;
        // R8
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