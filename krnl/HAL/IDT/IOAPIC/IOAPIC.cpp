#include <HAL/IDT/IOAPIC/IOAPIC.hpp>
#include <HAL/MEM/PMM.hpp>
#include <Library/debug.hpp>

namespace HAL::IDT::IOAPIC {
    static volatile uint32_t* ioapic_regsel = nullptr;
    static volatile uint32_t* ioapic_iowin  = nullptr;
    
    static ISOOverride iso_table[MAX_ISO_OVERRIDES] = {};

    void ioapic_write(uint8_t offset, uint32_t val) {
        *ioapic_regsel = offset;
        asm volatile("" ::: "memory");
        *ioapic_iowin  = val;
        asm volatile("" ::: "memory");
    }

    uint32_t ioapic_read(uint8_t offset) {
        *ioapic_regsel = offset;
        return *ioapic_iowin;
    }

    void initialize(uintptr_t base_addr) {
        ioapic_regsel = reinterpret_cast<volatile uint32_t*>(base_addr + REG_SEL_OFFSET + MEM::PMM::hhdm_offset);
        ioapic_iowin  = reinterpret_cast<volatile uint32_t*>(base_addr + IO_WIN_OFFSET + MEM::PMM::hhdm_offset);
        
        for (uint8_t i = 0; i < MAX_ISO_OVERRIDES; ++i) {
            iso_table[i] = { .gsi = i, .flags = 0, .exists = false };
        }
    }

    void register_iso(uint8_t irq, uint32_t gsi, uint16_t flags) {
        if (irq < MAX_ISO_OVERRIDES) {
            iso_table[irq] = { .gsi = gsi, .flags = flags, .exists = true };
        }
    }

    void set_redirect(uint8_t irq, uint8_t vector, uint8_t target_lapic_id, bool masked) {
        uint32_t gsi = irq;
        uint16_t flags = 0;

        if (irq < MAX_ISO_OVERRIDES && iso_table[irq].exists) {
            gsi = iso_table[irq].gsi;
            flags = iso_table[irq].flags;
        }

        uint8_t low_index  = IOREDTBL_BASE + (gsi * 2);
        uint8_t high_index = low_index + 1;

        uint8_t polarity_raw = flags & 0x3;
        uint32_t polarity_bit = (polarity_raw == 3) ? POLARITY_LOW : 0;

        uint8_t trigger_raw = (flags >> 2) & 0x3;
        uint32_t trigger_bit = (trigger_raw == 3) ? TRIGGER_LEVEL : TRIGGER_EDGE;

        uint32_t low_bits = vector | DELIVERY_FIXED | DEST_PHYSICAL | polarity_bit | trigger_bit;
        uint32_t high_bits = (static_cast<uint32_t>(target_lapic_id) << 24);

        ioapic_write(low_index, low_bits | MASKED_BIT);
        ioapic_write(high_index, high_bits);

        if (!masked) {
            ioapic_write(low_index, low_bits);
        }
    }
}