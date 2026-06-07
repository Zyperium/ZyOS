#pragma once
#include <Scheduler/Scheduler.hpp>
#include <stdint.h>

namespace HAL::CORE {
    struct ThreadLocal {
        Scheduler::Task *current_task;
        uint64_t kernel_stack;
        int core_id;
    } __attribute__((packed));
}