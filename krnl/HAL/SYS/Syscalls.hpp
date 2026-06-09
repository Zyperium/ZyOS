#pragma once
#include <stdint.h>

namespace HAL::SYS {
    enum class SYSCALLS {
        SYS_OPEN,
        SYS_CLOSE,
        SYS_READ,
        SYS_WRITE,
        SYS_LSEEK,
        SYS_MMAP,
        SYS_MUNMAP,
        SYS_FASTPIPE,
        SYS_KILL,
        SYS_GETPID,
        SYS_GETTIME,
        SYS_SHAREPAGE
    };

    struct REGISTERS {
        uint64_t a0, a1, a2;
        uint64_t a3, a4;
    };
}