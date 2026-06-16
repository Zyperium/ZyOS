#pragma once
#include <stddef.h>

namespace Debug {
    enum LogLevel {
        LOG_INFO,
        LOG_WARN,
        LOG_ERROR
    };

    void krnl_print(const char* class_name,
                LogLevel level,
                const char* fmt, ...);

    int snprintf(char* buffer, size_t n, 
                const char* fmt, ...);
}