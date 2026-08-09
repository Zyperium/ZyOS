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
        SYS_OPEN = 0,
        SYS_READ,
        SYS_WRITE,
        SYS_CLOSE,
        SYS_IOCTL,
        SYS_LOUT,

        SYS_MMAP,
        SYS_MUNMAP,
        SYS_MPROTECT,

        SYS_EXIT,
        SYS_YIELD,
        SYS_SPAWN,
        SYS_SET_FS_BASE,

        SYS_SHM_CREATE,
        SYS_SHM_MAP,
        SYS_SHM_UNMAP,

        SYS_FUTEX_WAIT,
        SYS_FUTEX_WAKE,

        SYS_GET_TIME,
        SYS_GET_PID,

        MAX_ID
    };

    void initialize();

    constexpr uint64_t MSR_STAR_VAL = 0x00130008ULL << 32;
    constexpr uint64_t MSR_FSTAR_VAL = 0x202; // clean RFLAGS
}