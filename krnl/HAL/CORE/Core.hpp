#pragma once
#include <HAL/CORE/ThreadLocal.hpp>
#include <stddef.h>

namespace HAL::CORE {
    void init_core(ThreadLocal *data);
    size_t get_core_id();
}