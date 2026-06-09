#pragma once
#include <stdint.h>

// These are all defined in assembly.
extern "C" void ignore_handler();
extern "C" void isr0();
extern "C" void isr1();
extern "C" void isr2();
extern "C" void isr8();
extern "C" void isr13();
extern "C" void isr14();
extern "C" void SchedulerHandler();
extern "C" void QuietSwitch();

namespace HAL::IDT {
    struct InterruptFrame {
        uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
        uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;

        uint64_t int_number;
        uint64_t error_code;

        uint64_t rip;
        uint64_t cs;
        uint64_t rflags;
        uint64_t rsp;
        uint64_t ss;
    } __attribute__((packed));

    struct IDTEntry {
        uint16_t offset_low;
        uint16_t selector;
        uint8_t ist;
        uint8_t attributes;
        uint16_t offset_mid;
        uint32_t offset_high;
        uint32_t zero;
    } __attribute__ ((packed));

    struct IDTR {
        uint16_t limit;
        uint64_t base;
    } __attribute__((packed));

    constexpr uint8_t GATE_INTERRUPT = 0x8E;
    constexpr uint8_t GATE_TRAP = 0x8E;
    constexpr uint8_t GATE_USER = 0xEE;

    constexpr uint64_t AMASK_LOW_16  = 0x000000000000FFFFULL;
    constexpr uint64_t AMASK_MID_16  = 0x00000000FFFF0000ULL;
    constexpr uint64_t AMASK_HIGH_32 = 0xFFFFFFFF00000000ULL;

    constexpr int SHIFT_MID  = 16;
    constexpr int SHIFT_HIGH = 32;

    constexpr uint8_t MSIX_VECTOR = 0x40;
    constexpr uint8_t LAPIC_VECTOR = 0x89;
    constexpr uint8_t YIELD_VECTOR = 0x67;
    constexpr uint8_t IST_MASK = 0x07;
    constexpr uint16_t MAX_VECTORS = 256;

    enum class ISR_CODES : uint8_t {
        DIV_ZERO,
        DEBUG_ERROR,
        NON_MASKABLE_INTERRUPT,
        BREAKPOINT,
        OVERFLOW,
        BOUND_RANGE,
        INVALID_OPCODE,
        DEV_NOT_AVAILABLE,
        DOUBLE_FAULT,
        INVALID_TSS = 10,
        SEGMENT_NOT_PRESENT,
        STACK_SEGMENT_FAULT,
        GENERAL_PROTECTION_FAULT,
        PAGE_FAULT,
        x87_FLOAT_POINT_EXCEPTION = 16
    };

    inline bool operator==(ISR_CODES a, uint8_t b) {
        return static_cast<uint8_t>(a) == b;
    }
    
    void initialize();
    void reload_idt();

    extern uintptr_t lapic_base_ptr;

    static inline void lapic_write(uint32_t reg, uint32_t value) {
        *((volatile uint32_t*)(lapic_base_ptr + reg)) = value;
    }

    static inline uint32_t lapic_read(uint32_t reg) {
        return *((volatile uint32_t*)(lapic_base_ptr + reg));
    }

    static inline uint16_t get_code_segment() {
        uint16_t cs;
        asm volatile("mov %%cs, %0" : "=r"(cs));
        return cs;
    }
}