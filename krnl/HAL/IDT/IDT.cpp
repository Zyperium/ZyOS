#include <HAL/IDT/IDT.hpp>
#include <HAL/IDT/Panic.hpp>

#include <Library/debug.hpp>
#include <Library/io.hpp>
// Right, i am writing everything out so I don't get confused. This should:
// Init:
// Syscalls, crash handlers, xHCI, LAPIC timers.
// Cool.
using namespace HAL::IDT;
extern "C" void exception_handler(HAL::IDT::InterruptFrame *frame) {

    switch (static_cast<ISR_CODES>(frame->int_number)) {
        case ISR_CODES::DIV_ZERO:
            panic(PanicReasons::DIVIDE_BY_ZERO_ERROR, frame);
            break;
        case ISR_CODES::GENERAL_PROTECTION_FAULT:
            panic(PanicReasons::GENERAL_FAULT_KMODE, frame);
            break;
        case ISR_CODES::PAGE_FAULT:
            panic(PanicReasons::PAGE_FAULT_KMODE, frame);
            break;
        default:
            break;
    }

    panic(PanicReasons::UNKNOWN_ERROR_CODE, frame);

    for (;;);
}

namespace HAL::IDT {
    static IDTEntry idt[MAX_VECTORS];
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

    void initialize() {
        Debug::krnl_print("IDT", Debug::LOG_INFO, "Initialize");
        for (int i = 0; i < MAX_VECTORS; i++) {
            set_gate(i, (void*)ignore_handler, GATE_INTERRUPT);
        }

        set_gate(ISR_CODES::DIV_ZERO, (void*)isr0, GATE_INTERRUPT);
        set_gate(ISR_CODES::GENERAL_PROTECTION_FAULT, (void*)isr13, GATE_INTERRUPT);
        set_gate(ISR_CODES::PAGE_FAULT, (void*)isr14, GATE_INTERRUPT);

        set_gate(LAPIC_VECTOR, (void*)SchedulerHandler, GATE_INTERRUPT);

        idtr.limit = (sizeof(IDTEntry) * MAX_VECTORS) - 1;
        idtr.base  = reinterpret_cast<uint64_t>(&idt[0]);

        asm volatile("lidt %0" :: "m"(idtr));        
    }
}