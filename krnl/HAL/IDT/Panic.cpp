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

    Debug::krnl_print("IDT", Debug::LOG_WARN, "The Operating System has encountered a severe error and will now terminate.");

    Debug::krnl_print("REG", Debug::LOG_INFO, "R15: %x", iframe->r15);
    Debug::krnl_print("REG", Debug::LOG_INFO, "R14: %x", iframe->r14);
    Debug::krnl_print("REG", Debug::LOG_INFO, "R13: %x", iframe->r13);
    Debug::krnl_print("REG", Debug::LOG_INFO, "R12: %x", iframe->r12);
    Debug::krnl_print("REG", Debug::LOG_INFO, "R11: %x", iframe->r11);
    Debug::krnl_print("REG", Debug::LOG_INFO, "R10: %x", iframe->r10);
    Debug::krnl_print("REG", Debug::LOG_INFO, "R9:  %x", iframe->r9);
    Debug::krnl_print("REG", Debug::LOG_INFO, "R8:  %x", iframe->r8);

    Debug::krnl_print("REG", Debug::LOG_INFO, "RBP: %x", iframe->rbp);
    Debug::krnl_print("REG", Debug::LOG_INFO, "RDI: %x", iframe->rdi);
    Debug::krnl_print("REG", Debug::LOG_INFO, "RSI: %x", iframe->rsi);
    Debug::krnl_print("REG", Debug::LOG_INFO, "RDX: %x", iframe->rdx);
    Debug::krnl_print("REG", Debug::LOG_INFO, "RCX: %x", iframe->rcx);
    Debug::krnl_print("REG", Debug::LOG_INFO, "RBX: %x", iframe->rbx);
    Debug::krnl_print("REG", Debug::LOG_INFO, "RAX: %x", iframe->rax);

    Debug::krnl_print("REG", Debug::LOG_INFO, "INT: %x", iframe->int_number);
    Debug::krnl_print("REG", Debug::LOG_INFO, "ERR: %x", iframe->error_code);

    Debug::krnl_print("REG", Debug::LOG_INFO, "RIP: %x", iframe->rip);
    Debug::krnl_print("REG", Debug::LOG_INFO, "CS:  %x", iframe->cs);
    Debug::krnl_print("REG", Debug::LOG_INFO, "FLG: %x", iframe->rflags);
    Debug::krnl_print("REG", Debug::LOG_INFO, "RSP: %x", iframe->rsp);
    Debug::krnl_print("REG", Debug::LOG_INFO, "SS:  %x", iframe->ss);

    for (;;);
}
