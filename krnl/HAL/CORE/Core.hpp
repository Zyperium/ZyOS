#pragma once
#include <HAL/CORE/CoreLocal.hpp>
#include <stddef.h>

namespace HAL::CORE {
    void init_core(CoreLocal *data);
    void discover_all_cores();
    CoreLocal *get_core_data();
    void init_lapic();
    void ack_lapic();
    void set_lapic_shot(uint64_t milliseconds);
    void broadcast_nmi();
    bool validate_gs_reg();
    extern void(*idleptr)();

    constexpr uint32_t LAPIC_SPURIOUS_VECTOR_MASK = 0xFF;
    constexpr uint32_t LAPIC_APIC_SOFTWARE_ENABLE = 0x100;
    constexpr uint32_t MASK_PIT_TIMER = 0xFF;

    extern volatile uint16_t core_count;
    extern volatile uint16_t total_cores;

    extern uintptr_t lapic_base_ptr;

    inline void lapic_write(uint32_t reg, uint32_t value) {
        *((volatile uint32_t*)(lapic_base_ptr + reg)) = value;
    }

    inline uint32_t lapic_read(uint32_t reg) {
        return *((volatile uint32_t*)(lapic_base_ptr + reg));
    }

    constexpr uint32_t LAPIC_ID = 0x020;
    constexpr uint32_t LAPIC_EOI = 0x0B0;
    constexpr uint32_t LAPIC_SPURIOUS = 0x0F0;
    constexpr uint32_t LAPIC_LVT_TIMER = 0x320;
    constexpr uint32_t LAPIC_PRIORITY_REG = 0x080;
    constexpr uint32_t LAPIC_TIMER_INITCNT = 0x380;
    constexpr uint32_t LAPIC_TIMER_CURCNT = 0x390;
    constexpr uint32_t LAPIC_TIMER_DIV = 0x3E0;
    constexpr uint32_t LAPIC_DIV_16 = 0x03;
    constexpr uint32_t LAPIC_INIT_COUNT = 0xFFFFFFFF;
    constexpr uint64_t LAPIC_ADDR_MASK = 0x0000FFFFFFFFF000ULL;
    constexpr uint32_t LAPIC_LVT_MASKED = 0x10000;
    constexpr uint32_t LAPIC_PERODIC_MODE = (1 << 17);
    constexpr uint32_t LAPIC_ONE_SHOT_MODE = 0;

    constexpr uint32_t LAPIC_ICR_LOW  = 0x300;
    constexpr uint32_t LAPIC_ICR_HIGH = 0x310;
    constexpr uint32_t LAPIC_ICR_DELIVERY_NMI = (4 << 8);
    constexpr uint32_t LAPIC_ICR_LEVEL_ASSERT = (1 << 14);
    constexpr uint32_t LAPIC_ICR_SHORTHAND_ALL_BUT_SELF = (3 << 18);
    constexpr uint32_t LAPIC_ICR_SEND_PENDING = (1 << 12);

    namespace PIT {
        constexpr uint16_t PORT_CHANNEL0= 0x40;
        constexpr uint16_t PORT_COMMAND= 0x43;

        constexpr uint8_t CMD_CHANNEL0 = 0x00;
        constexpr uint8_t CMD_ACCESS_LO_HI = 0x30;
        constexpr uint8_t CMD_MODE0_INT_ON_TC = 0x00;
        constexpr uint8_t CMD_BINARY_MODE = 0x00;
        constexpr uint8_t CONFIG_CALIBRATION_MODE = CMD_CHANNEL0 | CMD_ACCESS_LO_HI | CMD_MODE0_INT_ON_TC | CMD_BINARY_MODE;
        constexpr uint8_t CONFIG_LATCH_COMMAND = CMD_CHANNEL0;
        constexpr uint32_t BASE_FREQUENCY_HZ = 1193182;
        constexpr uint32_t TARGET_PERIOD_MS = 10;
        constexpr uint16_t RELOAD_VALUE = BASE_FREQUENCY_HZ / (1000 / TARGET_PERIOD_MS);
    }
}