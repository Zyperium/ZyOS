#pragma once
#include <stdint.h>
#include <stddef.h>

namespace HAL::IDT::IOAPIC {
    constexpr uintptr_t DEFAULT_IOAPIC_BASE = 0xFEC00000;

    constexpr uint16_t REG_SEL_OFFSET = 0x00;
    constexpr uint16_t IO_WIN_OFFSET  = 0x10;

    constexpr uint8_t IOREG_ID     = 0x00;
    constexpr uint8_t IOREG_VER    = 0x01;
    constexpr uint8_t IOREG_ARB    = 0x02;
    constexpr uint8_t IOREDTBL_BASE = 0x10;

    constexpr uint32_t DELIVERY_FIXED       = (0U << 8);
    constexpr uint32_t DELIVERY_LOWEST_PRIO = (1U << 8);
    constexpr uint32_t DEST_PHYSICAL        = (0U << 11);
    constexpr uint32_t DEST_LOGICAL         = (1U << 11);
    constexpr uint32_t POLARITY_HIGH        = (0U << 13);
    constexpr uint32_t POLARITY_LOW         = (1U << 13);
    constexpr uint32_t TRIGGER_EDGE         = (0U << 15);
    constexpr uint32_t TRIGGER_LEVEL        = (1U << 15);
    constexpr uint32_t MASKED_BIT           = (1U << 16);
    constexpr uint8_t MAX_ISO_OVERRIDES = 16;

    struct ISOOverride {
        uint32_t gsi;
        uint16_t flags;
        bool exists;
    };

    void initialize(uintptr_t base_addr = DEFAULT_IOAPIC_BASE);
    void debug_dump_keyboard_gsi();
    void set_redirect(uint8_t irq, uint8_t vector, uint8_t target_lapic_id, bool masked = false);
    void register_iso(uint8_t irq, uint32_t gsi, uint16_t flags);
}