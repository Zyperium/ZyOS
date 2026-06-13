#include <HAL/IDT/IOAPIC/IOAPIC.hpp>
#include <HAL/MEM/PMM.hpp>

#include <Library/debug.hpp>

namespace HAL::IDT::IOAPIC {
    static volatile uint32_t* ioapic_regsel = nullptr;
    static volatile uint32_t* ioapic_iowin  = nullptr;

    void ioapic_write(uint8_t offset, uint32_t val) {
        *ioapic_regsel = offset;
        *ioapic_iowin  = val;
    }

    uint32_t ioapic_read(uint8_t offset) {
        *ioapic_regsel = offset;
        return *ioapic_iowin;
    }

    void initialize(uintptr_t base_addr) {
        ioapic_regsel = reinterpret_cast<volatile uint32_t*>(base_addr + REG_SEL_OFFSET + MEM::PMM::hhdm_offset);
        ioapic_iowin  = reinterpret_cast<volatile uint32_t*>(base_addr + IO_WIN_OFFSET + MEM::PMM::hhdm_offset);
    }

    void debug_dump_keyboard_gsi() {
        uint32_t low_bits = ioapic_read(0x12);
        
        bool masked = (low_bits & (1 << 16)) != 0;
        bool delivery_status = (low_bits & (1 << 14)) != 0; 

        Debug::krnl_print("IOAPIC", Debug::LOG_INFO, 
            "IRQ1 State -> Vector: %x, Masked: %s, Pending: %s", 
            (low_bits & 0xFF), 
            masked ? "YES" : "NO", 
            delivery_status ? "YES (Stuck waiting for EOI!)" : "NO (Idle/Ready)"
        );
    }

    void set_redirect(uint8_t irq, uint8_t vector, uint8_t target_lapic_id, bool masked) {
        uint8_t low_index  = IO_WIN_OFFSET + (irq * 2);
        uint8_t high_index = low_index + 1;
        uint32_t low_bits = 0;

        low_bits |= vector;

        low_bits |= DELIVERY_MODE;
        low_bits |= DESTINATION_MODE;
        low_bits |= PIN_POLARITY;
        low_bits |= TRIGGER_MODE;

        if (masked) {
            low_bits |= MASKED_BIT;
        }

        uint32_t high_bits = 0;
        high_bits |= (static_cast<uint32_t>(target_lapic_id) << 24);

        ioapic_write(low_index, low_bits);
        ioapic_write(high_index, high_bits);
    }
}