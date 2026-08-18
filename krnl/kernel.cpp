#include <stdint.h>
#include <limine.h>

#include <HAL/HAL.hpp>
#include <HAL/IDT/Panic.hpp>
#include <HAL/SCREEN/Screen.hpp>
#include <HAL/PCI/xHCI/msix_xhci.hpp>
#include <HAL/CORE/Core.hpp>
#include <HAL/PS2/PS2KB.hpp>
#include <HAL/PS2/PS2Mouse.hpp>
#include <HAL/IDT/IOAPIC/IOAPIC.hpp>

#include <Services/ELF/KModule/KModule.hpp>
#include <Services/Scheduler/Scheduler.hpp>
#include <Services/Syscalls/Syscalls.hpp>
#include <Services/Code/Decomp.hpp>
#include <Services/SysInitA/SysInitA.hpp>

#include <Library/debug.hpp>
#include <Library/regs.h>

unsigned long __stack_chk_guard = 0xDEADDEADDEADDEAD;

extern "C" void __stack_chk_fail() {
    uint64_t return_address = (uint64_t)__builtin_return_address(0);

    Debug::krnl_print("IDT", Debug::LOG_ERROR, "Stack check failed! Caller RIP: %x", return_address);
    panic(PanicReasons::STACK_KERNEL_CORRUPTION);
}

using namespace HAL;

void SysIdleTask() {
    Debug::krnl_print("KRNL", Debug::LOG_INFO, "Entering idle task...");
    for (;;) {
        asm volatile("sti; hlt");
    }
}

void Reaper() {
    Scheduler::reaper_task = HAL::CORE::get_core_data()->current_task;
    for (;;) {
        Scheduler::ClearGarbage();
        Scheduler::reaper_task->block(Scheduler::BlockReasons::SLEEP);
        asm volatile("pause");
    }
}

extern "C" void krnlmain() {
    initialize();

    Scheduler::Initialize();

    Scheduler::Task *krnl_task = new Scheduler::Task((Scheduler::Task::EntryPoint)SysIdleTask, "System Idle Task", false);
    Debug::krnl_print("KRNL", Debug::LOG_INFO, "Created task with PID %i", krnl_task->get_pid());
    HAL::CORE::get_core_data()->system_idle_task = krnl_task;
    HAL::CORE::idleptr = SysIdleTask;

    Syscalls::initialize();

    new Scheduler::Task(
        (Scheduler::Task::EntryPoint)Reaper, 
        "Reaper", 
        true
    );

    new Scheduler::Task(
        (Scheduler::Task::EntryPoint)HAL::CORE::discover_all_cores, 
        "CoreFinder",
        true
    );

    // while (HAL::CORE::total_cores != HAL::CORE::core_count) {
    //     asm volatile("pause");
    // }

    HAL::PCI::MSIX::xHCI::create_xhci_worker();

    new Scheduler::Task(
        (Scheduler::Task::EntryPoint)ELF::KModule::initialize, 
        "KMODULE", 
        true
    );

    new Scheduler::Task(
        (Scheduler::Task::EntryPoint)Scheduler::ForkerTask,
        "Forker",
        true
    );

    PS2::Keyboard::Initialize();
    PS2::Mouse::Initialize();

    Debug::krnl_print("KRNL", Debug::LOG_INFO, "Enabling scheduler");
    Scheduler::EnableScheduler();

    // IDT::IOAPIC::debug_dump_keyboard_gsi();

    asm volatile("sti");

    for (;;) {
        asm volatile("hlt");
    }
}