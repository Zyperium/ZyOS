#pragma once
#include <stdint.h>
#include <stddef.h>

namespace HAL::IDT::IOAPIC {
    constexpr uintptr_t DEFAULT_IOAPIC_BASE = 0xFEC00000;
    constexpr uint16_t REG_SEL_OFFSET = 0x0;
    constexpr uint16_t IO_WIN_OFFSET = 0x10;

    constexpr uint64_t DELIVERY_MODE = (0 << 8);
    constexpr uint64_t DESTINATION_MODE = (0 << 8);
    constexpr uint64_t PIN_POLARITY = (0 << 13);
    constexpr uint64_t TRIGGER_MODE = (0 << 15);
    constexpr uint64_t MASKED_BIT = (1 << 16);

    void initialize(uintptr_t base_addr = DEFAULT_IOAPIC_BASE);
    void debug_dump_keyboard_gsi();
    void set_redirect(uint8_t irq, uint8_t vector, uint8_t target_lapic_id, bool masked = false);
}