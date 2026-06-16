#pragma once
#include <stddef.h>
#include <stdint.h>
#include <Library/cystr.hpp>

namespace ELF::KModule {
    void initialize();
    void export_symbol(const char *symbol, uint64_t kernel_address);
    uint64_t get_symbol(void *handle, const char *symbol);
    void *load_module(lib::string path);

    constexpr uint16_t MIN_PATH_LENGTH = 3;
}