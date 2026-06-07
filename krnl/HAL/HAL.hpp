#pragma once
#include <stddef.h>

namespace HAL {
    void initialize();

    void *kmalloc(size_t size);
    void kfree(void *addr);
}