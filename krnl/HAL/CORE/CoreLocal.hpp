#pragma once
#include <Services/Scheduler/Scheduler.hpp>
#include <stdint.h>
#include <Library/ZyOS.hpp>

namespace HAL::CORE {
    struct alignas(ZyOS::sbQWORD) CoreLocal {
        CoreLocal *self;
        Scheduler::Task *current_task;
        uint64_t last_task_runtime;
        uint64_t r10_save;
        uint64_t kernel_stack;
        int core_id;
        uint32_t lapic_ticks_per_ms;
        Scheduler::Task *system_idle_task;
    };
}