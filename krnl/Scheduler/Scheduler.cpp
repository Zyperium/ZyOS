#include <stddef.h>
#include <stdint.h>

#include <Scheduler/Scheduler.hpp>

namespace Scheduler {
    bool active;

    Task *tasks[32];
}

extern "C" uint64_t SchedulerSwitch(uint64_t current_rsp) {
    if (!Scheduler::active) {
        return current_rsp;
    }

    uint64_t next_rsp;

    return next_rsp;
}