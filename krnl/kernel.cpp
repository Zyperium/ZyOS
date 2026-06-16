#include <stdint.h>
#include <limine.h>

#include <HAL/HAL.hpp>
#include <HAL/IDT/Panic.hpp>
#include <HAL/SCREEN/Screen.hpp>
#include <HAL/PCI/xHCI/msix_xhci.hpp>
#include <HAL/CORE/Core.hpp>
#include <HAL/PS2/PS2KB.hpp>
#include <HAL/IDT/IOAPIC/IOAPIC.hpp>

#include <Services/TTY/TTY.hpp>
#include <Scheduler/Scheduler.hpp>

#include <Library/debug.hpp>
#include <Library/regs.h>
unsigned long __stack_chk_guard = 0xDEADDEADDEADDEAD;

extern "C" void __stack_chk_fail() {
    panic(PanicReasons::STACK_KERNEL_CORRUPTION);
}

using namespace HAL;

void SysIdleTask() {
    Debug::krnl_print("KRNL", Debug::LOG_INFO, "Sysidle running on core %i", HAL::CORE::get_thread_data()->core_id);
    Debug::krnl_print("KRNL", Debug::LOG_INFO, "Task was created on core %i", HAL::CORE::get_thread_data()->system_idle_task->current_core);
    for (;;) {
        Scheduler::Task *stolen = Scheduler::StealCoCoreTask();

        if (stolen) {
            Scheduler::Yield();
        }

        asm volatile("hlt");
    }
}

void TTY_Task(void *tty_cast) {
    static_cast<TTY::ConHost *>(tty_cast)->worker();
}

extern "C" void krnlmain() {
    initialize();

    Scheduler::Initialize();

    Scheduler::Task *krnl_task = new Scheduler::Task((Scheduler::Task::EntryPoint)SysIdleTask, "System Idle Task", false);
    Debug::krnl_print("KRNL", Debug::LOG_INFO, "Created task with PID %i", krnl_task->get_pid());
    HAL::CORE::get_thread_data()->system_idle_task = krnl_task;
    HAL::CORE::idleptr = SysIdleTask;

    HAL::CORE::discover_all_cores();

    while (HAL::CORE::total_cores != HAL::CORE::core_count) {
        asm volatile("pause");
    }

    HAL::PCI::MSIX::xHCI::create_xhci_worker();

    TTY::ConHost *default_host = new TTY::ConHost;
    default_host->contask = new Scheduler::Task((Scheduler::Task::EntryPoint)TTY_Task, "TTY0", true, default_host);
    default_host->contask->core_pinned = true;

    uint64_t rflags;

    asm volatile("pushfq; pop %0" : "=r"(rflags));
    if (rflags & (1 << 9)) {
        Debug::krnl_print("KRNL", Debug::LOG_INFO, "Interrupts enabled too soon?");
    }

    PS2::Keyboard::Initialize();

    Debug::krnl_print("KRNL", Debug::LOG_INFO, "Enabling scheduler");
    Scheduler::EnableScheduler();

    IDT::IOAPIC::debug_dump_keyboard_gsi();

    asm volatile("sti");

    for (;;) {
        asm volatile("hlt");
    }
}