#pragma once
#include <HAL/CORE/ThreadLocal.hpp>
#include <stddef.h>

namespace HAL::CORE {
    void init_core(ThreadLocal *data);
    ThreadLocal *get_thread_data();
}