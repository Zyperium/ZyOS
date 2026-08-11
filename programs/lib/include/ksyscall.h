#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

static inline uint64_t syscall_base(uint64_t ID, uint64_t A1, uint64_t A2, 
                                     uint64_t A3, uint64_t A4, uint64_t A5, 
                                     uint64_t A6) {
    uint64_t ret;
    register uint64_t rax_val asm("rax") = ID;
    register uint64_t rdi_val asm("rdi") = A1;
    register uint64_t rsi_val asm("rsi") = A2;
    register uint64_t rdx_val asm("rdx") = A3;
    register uint64_t r10_val asm("r10") = A4;
    register uint64_t r8_val  asm("r8")  = A5;
    register uint64_t r9_val  asm("r9")  = A6;

    asm volatile(
        "syscall"
        : "=a"(ret)
        : "r"(rax_val), "r"(rdi_val), "r"(rsi_val), 
          "r"(rdx_val), "r"(r10_val), "r"(r8_val), "r"(r9_val)
        : "rcx", "r11", "memory"
    );

    return ret;
}
#define _SYSCALL_RESOLVE(_1, _2, _3, _4, _5, _6, _7, NAME, ...) NAME

#define syscall(...) _SYSCALL_RESOLVE(__VA_ARGS__, \
    _syscall_7, _syscall_6, _syscall_5, _syscall_4, _syscall_3, _syscall_2, _syscall_1)(__VA_ARGS__)

#define _syscall_1(id)                         syscall_base((uint64_t)(id), 0, 0, 0, 0, 0, 0)
#define _syscall_2(id, a1)                     syscall_base((uint64_t)(id), (uint64_t)(a1), 0, 0, 0, 0, 0)
#define _syscall_3(id, a1, a2)                 syscall_base((uint64_t)(id), (uint64_t)(a1), (uint64_t)(a2), 0, 0, 0, 0)
#define _syscall_4(id, a1, a2, a3)             syscall_base((uint64_t)(id), (uint64_t)(a1), (uint64_t)(a2), (uint64_t)(a3), 0, 0, 0)
#define _syscall_5(id, a1, a2, a3, a4)         syscall_base((uint64_t)(id), (uint64_t)(a1), (uint64_t)(a2), (uint64_t)(a3), (uint64_t)(a4), 0, 0)
#define _syscall_6(id, a1, a2, a3, a4, a5)     syscall_base((uint64_t)(id), (uint64_t)(a1), (uint64_t)(a2), (uint64_t)(a3), (uint64_t)(a4), (uint64_t)(a5), 0)
#define _syscall_7(id, a1, a2, a3, a4, a5, a6) syscall_base((uint64_t)(id), (uint64_t)(a1), (uint64_t)(a2), (uint64_t)(a3), (uint64_t)(a4), (uint64_t)(a5), (uint64_t)(a6))

#ifdef __cplusplus
}
#endif