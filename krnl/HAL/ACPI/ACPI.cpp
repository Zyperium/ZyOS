#include <Library/debug.hpp>
#include <Library/string.h>
#include <Library/regs.h>
#include <HAL/ACPI/HPET.hpp>
#include <HAL/ACPI/ACPI.hpp>
#include <HAL/IDT/Panic.hpp>
#include <HAL/MEM/PMM.hpp>
#include <HAL/MEM/PMEM.hpp>
#include <HAL/MEM/VMM.hpp>
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
    uint32_t hpet_speed;
    volatile uint64_t *hpet;

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
        bool use_xsdt = (rsdp->rsdp.revision >= 2 && rsdp->xsdt_address != 0);

        size_t entries = 0;
        uintptr_t tables_base = 0;

        if (use_xsdt) {
            auto xsdt_phys = rsdp->xsdt_address;
            auto *xsdt = reinterpret_cast<XSDT *>(xsdt_phys + PMM::hhdm_offset);

            Debug::krnl_print("ACPI", Debug::LOG_INFO, "Found XSDT at %xP %xV", xsdt_phys, xsdt);

            entries = (xsdt->header.length - sizeof(ACPISDT)) / sizeof(uint64_t);
            tables_base = reinterpret_cast<uintptr_t>(xsdt + 1);
        } else {
            Debug::krnl_print("ACPI", Debug::LOG_INFO, "Version is < 2, using RSDT");

            auto *rsdt = reinterpret_cast<RSDT *>(rsdp->rsdp.rsdt_address + PMM::hhdm_offset);
            entries = (rsdt->header.length - sizeof(ACPISDT)) / sizeof(uint32_t);
            tables_base = reinterpret_cast<uintptr_t>(rsdt + 1);
        }

        Debug::krnl_print("ACPI", Debug::LOG_INFO, "Found %i entries", entries);

        for (auto i{0uz}; i < entries; ++i) {
            uintptr_t table_phys = use_xsdt
                ? reinterpret_cast<uint64_t *>(tables_base)[i]
                : reinterpret_cast<uint32_t *>(tables_base)[i];

            auto *table = reinterpret_cast<ACPISDT *>(table_phys + PMM::hhdm_offset);
            Debug::krnl_print("ACPI", Debug::LOG_INFO, "Identified table at address %x", table);

            char acpi_sig[ACPI_SIGN_LEN + 1] {
                table->signature[0],
                table->signature[1],
                table->signature[2],
                table->signature[3],
                0
            };

            Debug::krnl_print("ACPI", Debug::LOG_INFO, "Checking signature %s [%i of %i]", acpi_sig, i + 1, entries);

            if (strncmp(acpi_sig, "HPET", ACPI_SIGN_LEN)) {
                Debug::krnl_print("ACPI", Debug::LOG_INFO, "Found HPET");

                hpet_table = reinterpret_cast<HPET *>(table);
                Debug::krnl_print(
                    "ACPI", Debug::LOG_INFO, "HPET info: OEM ID %i, Addr %x, Signature %s",
                    hpet_table->header.oem_id, hpet_table->address, hpet_table->header.signature
                );

                hpet = (uint64_t *)PMEM::map_mmio((uintptr_t)hpet_table->address, 1);
                hpet[HPET_GCONF / HPET_EN_BIT] |= HPET_ON;
                hpet_speed = hpet[HPET_GCAP] >> HPET_SPEED_OFF;
                Debug::krnl_print("ACPI", Debug::LOG_INFO, "Speed: %i", hpet_speed);
            }
        }

        Debug::krnl_print("ACPI", Debug::LOG_INFO, "Finished enumeration");

        return;
    }

    uint64_t get_sys_time() {
        if (!hpet_speed) return 0;

        uint64_t raw_ticks = hpet[HPET_MAIN / HPET_TICKS_OFF];

        return (raw_ticks * hpet_speed) / TO_MS;
    }
}