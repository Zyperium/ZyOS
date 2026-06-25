#include <Library/debug.hpp>
#include <Library/string.h>
#include <HAL/ACPI/HPET.hpp>
#include <HAL/ACPI/ACPI.hpp>
#include <HAL/IDT/Panic.hpp>
#include <HAL/MEM/PMM.hpp>
#include <stdint.h>
#include <limine.h>

static volatile constinit struct limine_rsdp_request rsdp_request = {
    .id = LIMINE_RSDP_REQUEST_ID,
    .revision = 0,
    .response = nullptr
};

using namespace HAL::MEM;

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

    void init() {
        if (rsdp_request.response == nullptr || rsdp_request.response->address == nullptr) {
            panic(PanicReasons::HAL_FAILED_INITIALIZATION);
        }

        auto *rsdp = static_cast<RSDP2 *>(rsdp_request.response->address);

        if (rsdp->rsdp.revision >= 2) {
            auto xsdt_phys = rsdp->xsdt_address;
            auto xsdt = reinterpret_cast<XSDT *>(xsdt_phys + PMM::hhdm_offset);

            Debug::krnl_print("ACPI", Debug::LOG_INFO, "Found xsdt at %xP %xV", xsdt_phys, xsdt);
            panic(PanicReasons::HAL_FAILED_INITIALIZATION);
        }

        Debug::krnl_print("ACPI", Debug::LOG_INFO, "Version is < 2");

        auto *rsdt = reinterpret_cast<RSDT *>(rsdp->rsdp.rsdt_address + PMM::hhdm_offset);
        auto entries = (rsdt->header.length - sizeof(ACPISDT)) / sizeof(uint32_t);

        Debug::krnl_print("ACPI", Debug::LOG_INFO, "Found %i entries", entries);

        for (auto i{0uz}; i < entries; ++i) {
            auto *table = (ACPISDT *)(uintptr_t)rsdt->tables[i];
            Debug::krnl_print("ACPI", Debug::LOG_INFO, "Identified table at address %x", table);
            char acpi_sig[ACPI_SIGN_LEN];

            strncpy(acpi_sig, table->signature, ACPI_SIGN_LEN);
            acpi_sig[ACPI_SIGN_LEN - 1] = 0;

            Debug::krnl_print("ACPI", Debug::LOG_INFO, "Checking signature %s", acpi_sig);
        }
    }

}