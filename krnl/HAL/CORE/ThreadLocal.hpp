#pragma once
#include <Scheduler/Scheduler.hpp>
#include <stdint.h>
#include <Library/ZyOS.hpp>

namespace HAL::CORE {
    struct alignas(ZyOS::sbQWORD) ThreadLocal {
        Scheduler::Task *current_task;
        Scheduler::Task *last_task;
        uint64_t kernel_stack;
        int core_id;
    };
}