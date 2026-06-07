#include <HAL/IDT/IDT.hpp>

#include <Library/debug.hpp>
#include <Library/io.hpp>
// Right, i am writing everything out so I don't get confused. This should:
// Init:
// Syscalls, crash handlers, xHCI, LAPIC timers.
// Cool.

extern "C" void exception_handler(HAL::IDT::InterruptFrame *frame) {
    if (HAL::IDT::ISR_CODES::PAGE_FAULT == frame->error_code) {

    }
}

namespace HAL::IDT {
    static IDTEntry idt[256];
    static IDTR idtr;
    uintptr_t lapic_base_ptr = 0;

    void set_gate(uint8_t vector, void* handler, uint8_t flags, uint8_t ist = 0) {
        const uint64_t addr = reinterpret_cast<uint64_t>(handler);

        idt[vector].offset_low  = static_cast<uint16_t>(addr & AMASK_LOW_16);
        idt[vector].selector    = get_code_segment();
        idt[vector].ist         = static_cast<uint8_t>(ist & IST_MASK);
        idt[vector].attributes  = flags;
        idt[vector].offset_mid  = static_cast<uint16_t>((addr & AMASK_MID_16) >> SHIFT_MID);
        idt[vector].offset_high = static_cast<uint32_t>((addr & AMASK_HIGH_32) >> SHIFT_HIGH);
        idt[vector].zero        = 0;
    }

    void set_gate(ISR_CODES isr_vector, void* handler, uint8_t flags, uint8_t ist = 0) {
        const uint64_t addr = reinterpret_cast<uint64_t>(handler);
        uint8_t vector  = static_cast<uint8_t>(isr_vector);

        idt[vector].offset_low  = static_cast<uint16_t>(addr & AMASK_LOW_16);
        idt[vector].selector    = get_code_segment();
        idt[vector].ist         = static_cast<uint8_t>(ist & IST_MASK);
        idt[vector].attributes  = flags;
        idt[vector].offset_mid  = static_cast<uint16_t>((addr & AMASK_MID_16) >> SHIFT_MID);
        idt[vector].offset_high = static_cast<uint32_t>((addr & AMASK_HIGH_32) >> SHIFT_HIGH);
        idt[vector].zero        = 0;
    }

    // FIX: magic numbers! Do this please!
    void init_pit(uint32_t freq) {
        uint32_t divisor = 1193180 / freq;
        outb(0x43, 0x36);
        outb(0x40, (uint8_t)(divisor & 0xFF));
        outb(0x40, (uint8_t)((divisor >> 8) & 0xFF));
    }

    void initialize() {
        Debug::krnl_print("IDT", Debug::LOG_INFO, "Initialize");
        for (int i = 0; i < 256; i++) {
            set_gate(i, (void*)ignore_handler, GATE_INTERRUPT);
        }

        set_gate(ISR_CODES::DIV_ZERO, (void*)isr0, GATE_INTERRUPT);

        asm volatile("lidt %0" :: "m"(idtr));
    }
}