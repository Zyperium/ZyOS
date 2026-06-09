#include <HAL/IDT/Panic.hpp>

#include <Library/debug.hpp>

void panic(PanicReasons reason, HAL::IDT::InterruptFrame *iframe) {
    Debug::krnl_print("IDT", Debug::LOG_INFO, "Panic!");
    switch (reason) {
        case PanicReasons::DIVIDE_BY_ZERO_ERROR:
            Debug::krnl_print("IDT", Debug::LOG_INFO, "Divide by zero error!");
            break;
        case PanicReasons::PAGE_FAULT_KMODE:
            Debug::krnl_print("IDT", Debug::LOG_INFO, "Page fault in kmode!");
            break;
        case PanicReasons::GENERAL_FAULT_KMODE: {
            Debug::krnl_print("IDT", Debug::LOG_INFO, "General protection fault!");
            Debug::krnl_print("SCHD", Debug::LOG_INFO, "IRETQ was popping:");
            uint64_t *stack = (uint64_t *)iframe->rsp;
            for (auto i{0uz}; i < 5; i++)
                Debug::krnl_print("SCHD", Debug::LOG_INFO, "%x", stack[i]);
            break;
        }
        default:
            Debug::krnl_print("IDT", Debug::LOG_INFO, "Unknown kmode error!");
            break;
    }

    if (!iframe) {
        Debug::krnl_print("IDT", Debug::LOG_INFO, "No iframe was provided.");
        for (;;);
    }

    Debug::krnl_print("IDT", Debug::LOG_WARN, "The Operating System has encountered a severe error and will now terminate.\nRIP: %x\nRSP: %x", iframe->rip, iframe->rsp);
    
    for (;;);
}
