#include <HAL/CORE/Core.hpp>
#include <HAL/IDT/Panic.hpp>
#include <HAL/SCREEN/Screen.hpp>

#include <Services/Code/Decomp.hpp>

#include <Library/debug.hpp>
#include <Library/osconfig.hpp>

void panic(PanicReasons reason, HAL::IDT::InterruptFrame *iframe) {
    HAL::SCREEN::fill_screen(HAL::SCREEN::COL::RED);
    HAL::SCREEN::flip_buffer();
    Debug::krnl_print("IDT", Debug::LOG_INFO, "Panic!");
    switch (reason) {
        case PanicReasons::DIVIDE_BY_ZERO_ERROR:
            Debug::krnl_print("IDT", Debug::LOG_INFO, "Divide by zero error!");
            break;
        case PanicReasons::PAGE_FAULT_KMODE:
            Debug::krnl_print("IDT", Debug::LOG_INFO, "Page fault in kmode! (%s fault)", HAL::CORE::get_core_data()->current_task->task_name.c_str());
            break;
        case PanicReasons::GENERAL_FAULT_KMODE: {
            Debug::krnl_print("IDT", Debug::LOG_INFO, "General protection fault!");
            Debug::krnl_print("SCHD", Debug::LOG_INFO, "Top stack:");
            uint64_t *stack = (uint64_t *)iframe->rsp;
            for (auto i{0uz}; i < 5; i++)
                Debug::krnl_print("SCHD", Debug::LOG_INFO, "%x", stack[i]);
            break;
        }
        case PanicReasons::STACK_KERNEL_CORRUPTION: {
            Debug::krnl_print("IDT", Debug::LOG_WARN, "Critical stack corruption detected.");
            break;
        }
        default:
            Debug::krnl_print("IDT", Debug::LOG_INFO, "Unknown kmode error! (%i)", reason);
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

    #if OSCONF_CRASH_DECOMPILER
    Debug::krnl_print("IDT", Debug::LOG_INFO, "Beginning debug dump");

    constexpr size_t kMaxAround = OSCONF_CRASH_LOG_COUNT;
    constexpr size_t kTotalLines = (kMaxAround * 2) + 1;

    Decomp::DisasmLine line_buffer[kTotalLines];
    Decomp::DisasmLine history_buffer[kMaxAround];

    Decomp::Disasmrep r{};
    r.lines = line_buffer;
    r.total_lines = kTotalLines;
    r.count = 0;

    if (Decomp::capture_disasm_stack(iframe->rip, &r, history_buffer, kMaxAround)) {
        for (auto i{0}; i < r.count; ++i) {
            Debug::krnl_print("DCMP", Debug::LOG_INFO, "%s %x: %s", 
                r.lines[i].is_rip ? "=>" : "  ", 
                (void *)r.lines[i].addr, 
                r.lines[i].text
            );
        }
    } else {
        Debug::krnl_print("IDT", Debug::LOG_WARN, "Failed to decompile rip in fault handler");
    }
#endif

    for (;;) asm volatile ("hlt");
}
