#pragma once
#include <stdint.h>

namespace ACPI {
    struct RSDP {
        char signature[8];
        uint8_t checksum;
        char oem_id[6];
        uint8_t revision;
        uint32_t rsdt_address;
    };

    struct RSDP2 {
        RSDP rsdp;
        uint32_t length;
        uint64_t xsdt_address;
        uint8_t extended_checksum;
        uint8_t reserved[3];
    };

    struct ACPISDT {
        char signature[4];
        uint32_t length;
        uint8_t revision;
        uint8_t checksum;
        char oem_id[6];
        char oem_table_id[8];
        uint32_t oem_revision;
        uint32_t creator_id;
        uint32_t creator_revision;
    } __attribute__((packed));

    struct RSDT {
        ACPISDT header;
        // Tables are here, but really weird behaviour attempting to index them.
    } __attribute__((packed));

    struct XSDT {
        ACPISDT header;
        uint64_t *tables;
    } __attribute__((packed));

    void init();
    uint64_t get_sys_time();
    uint8_t get_apic_id();
    
    static constexpr uint32_t CPUID_EAX_FIRST_INFO      = 1;
    static constexpr uint32_t CPUID_EBX_APIC_ID_SHIFT   = 24;
    static constexpr uint32_t CPUID_EBX_APIC_ID_MASK    = 0xFF;
    static constexpr uint32_t ACPI_SIGN_LEN             = 4;
}