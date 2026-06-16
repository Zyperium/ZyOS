#pragma once
#include <stddef.h>
#include <stdint.h>
#include <Library/cystr.hpp>

namespace ELF::KModule {
    void initialize();
    void export_symbol(const char *symbol, uint64_t kernel_address);
    uint64_t resolve_symbol(const char *symbol);
    void *load_module(lib::string path);

    constexpr uint16_t MIN_ALIGN = 16;
    constexpr uint64_t RELA_INFO_SHIFT = 32;
    constexpr uint64_t SYM_INFO_SHIFT = 4;
    constexpr uint64_t RES_OFFSET = 4;
    constexpr uint64_t RELA_INFO_MASK = 0xFFFFFFFF;

    struct HashedElement {
        uint32_t hash;
        uint32_t padding;
        uint64_t address;
    } __attribute__((packed));

    struct HashTableHeader {
        uint64_t symbol_count;
        HashedElement *elements;
    } __attribute__((packed));
}