#pragma once
#include <stdint.h>

namespace HAL::MSR {
    constexpr uint64_t IA32_EFER = 0xC0000080;
    constexpr uint64_t IA32_STAR = 0xC0000081;
    constexpr uint64_t IA32_LSTAR = 0xC0000082;
    constexpr uint64_t IA32_CSTAR = 0xC0000083;
    constexpr uint64_t IA32_FMASK = 0xC0000084;
    constexpr uint64_t IA32_FS_BASE = 0xC0000100;
    constexpr uint64_t IA32_GS_BASE = 0xC0000101;
    constexpr uint64_t IA32_KERNEL_GS_BASE = 0xC0000102;
    constexpr uint64_t IA32_TSC_AUX = 0xC0000103;

    constexpr uint64_t IA32_TIME_STAMP_COUNTER = 0x00000010;
    constexpr uint64_t IA32_APIC_BASE = 0x0000001B;
    constexpr uint64_t IA32_FEATURE_CONTROL = 0x0000003A;
    constexpr uint64_t IA32_SYSENTER_CS = 0x00000174;
    constexpr uint64_t IA32_SYSENTER_ESP = 0x00000175;
    constexpr uint64_t IA32_SYSENTER_EIP = 0x00000176;

    static inline void wrmsr(uint64_t msr_id, uint64_t data) {
        uint32_t low = data & 0xFFFFFFFF;
        uint32_t high = data >> 32;

        asm volatile("wrmsr" : : "a"(low), "d"(high), "c"(msr_id));
    }

    static inline uint64_t rdmsr(uint64_t msr_id) {
        uint32_t low, high;
        asm volatile("rdmsr" : "=a"(low), "=d"(high) : "c"(msr_id));
        return ((uint64_t)high << 32) | low;
    }
}