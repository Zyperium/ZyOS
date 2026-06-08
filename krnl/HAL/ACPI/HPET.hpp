#pragma once
#include <stdint.h>
#include <HAL/ACPI/ACPI.hpp>

namespace ACPI {
    struct HPET {
        ACPISDT header;
        uint32_t event_timer_block_id;
        
        uint8_t address_space_id;
        uint8_t register_bit_width;
        uint8_t register_bit_offset;
        uint8_t reserved_gas;
        uint64_t address;

        uint8_t hpet_number;
        uint16_t minimum_tick;
        uint8_t page_protection;
    } __attribute__((packed));

    constexpr uint64_t HPET_GCAP = 0x0;
    constexpr uint64_t HPET_GCONF = 0x10;
    constexpr uint64_t HPET_MAIN = 0xF0;

    extern HPET *hpet_table;
}