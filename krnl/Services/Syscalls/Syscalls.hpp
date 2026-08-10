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
        // R9
        uint64_t A6;
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
        SYS_FORK,
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
    constexpr uint64_t EINVAL = 22;
    constexpr uint64_t ENOMEM = 12;

    #define MAP_FAILED ((void*)-1)
    #define MAP_FIXED 0x10
    #define MAP_ANONYMOUS 0x20
}