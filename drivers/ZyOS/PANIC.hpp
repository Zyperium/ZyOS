#pragma once
#include <stdint.h>

enum class PanicReasons {
    GENERAL_FAULT_KMODE,
    PAGE_FAULT_KMODE,
    HAL_FAILED_INITIALIZATION,
    THIRD_PARTY_DRIVER_ERROR,
    DIVIDE_BY_ZERO_ERROR,
    SCHEDULER_OUT_OF_TASKS,
    OUT_OF_MEMORY,
    OUT_OF_PIDs, // Note this is only raised if a driver tries to fork while no PIDs are available.
    UNKNOWN_ERROR_CODE,
    xHCI_CRITICAL_ERROR,
    STACK_KERNEL_CORRUPTION
};

void panic(PanicReasons reason, uint64_t iframe = 0);