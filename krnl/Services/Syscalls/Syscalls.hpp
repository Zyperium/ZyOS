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
        // R9
        uint64_t A6;
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
        SYS_OPEN = 0, // functional
        SYS_GLEN,
        SYS_READ, // functional
        SYS_WRITE, // functional
        SYS_CLOSE, // functional
        SYS_IOCTL, // functional
        SYS_LOUT, // functional

        SYS_MMAP, // functional
        SYS_MUNMAP, // functional
        SYS_MPROTECT, // stub, however MMAP supports setting permissions

        SYS_EXIT, // functional
        SYS_YIELD, // functional
        SYS_FORK, // functional
        SYS_SET_FS_BASE, // functional

        SYS_SHM_CREATE, // stub
        SYS_SHM_MAP, // stub
        SYS_SHM_UNMAP, // stub

        SYS_FUTEX_WAIT, // stub
        SYS_FUTEX_WAKE, // stub

        SYS_GET_TIME, // functional
        SYS_GET_PID, // functional
        SYS_EXEC_APP,

        MAX_ID
    };

    void initialize();
    char *usr_to_string(uint64_t usr_ptr, uint64_t max_value);

    constexpr uint64_t MSR_STAR_VAL = 0x00130008ULL << 32;
    constexpr uint64_t MSR_FSTAR_VAL = 0x202; // clean RFLAGS
    constexpr uint64_t EINVAL = 22;
    constexpr uint64_t ENOMEM = 12;

    #define MAP_FAILED ((void*)-1)
    #define MAP_FIXED 0x10
    #define MAP_ANONYMOUS 0x20
}