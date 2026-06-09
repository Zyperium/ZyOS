#include <stdint.h>
#include <limine.h>

#include <HAL/HAL.hpp>
#include <HAL/IDT/Panic.hpp>
#include <HAL/SCREEN/Screen.hpp>
#include <HAL/PCI/xHCI/msix_xhci.hpp>
#include <HAL/CORE/Core.hpp>

#include <Scheduler/Scheduler.hpp>

#include <Library/debug.hpp>
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

    asm volatile("sti");

    SCREEN::fill_screen(SCREEN::COL::GREEN);
    SCREEN::flip_buffer();

    Scheduler::EnableScheduler();

    for (;;) {
        asm volatile("hlt");
    }
}