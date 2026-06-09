#include <HAL/SYS/Syscalls.hpp>

namespace HAL::SYS {
    void SyscallHandler(SYSCALLS id, REGISTERS reg) {
        (void)id;
        (void)reg;
        return;
    }
}

extern "C" void SyscallStub(uint64_t id, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5) {
    HAL::SYS::SyscallHandler((HAL::SYS::SYSCALLS)id, {a1, a2, a3, a4, a5});\
    return;
}