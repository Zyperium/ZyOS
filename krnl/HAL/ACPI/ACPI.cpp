#include <HAL/ACPI/HPET.hpp>

namespace ACPI {
    HPET *hpet_table = nullptr;
    volatile uint64_t *hpet;
    uint32_t hpet_speed;

    uint8_t get_apic_id() {
        uint32_t ebx = 0;
    
        asm volatile (
            "cpuid"
            : "=b"(ebx)
            : "a"(CPUID_EAX_FIRST_INFO)
            : "ecx", "edx"
        );

        return (ebx >> CPUID_EBX_APIC_ID_SHIFT) & CPUID_EBX_APIC_ID_MASK;
    }

}