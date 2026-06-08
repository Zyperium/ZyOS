#pragma once
#include <stddef.h>
#include <stdint.h>

namespace HAL::MEM::PMEM {
    constexpr uint64_t PMEM_START_ADDR = 0xFFFFA00000000000;

    void initialize();
    void *alloc_page(uint64_t flags);
    void *alloc_pages(size_t count, uint64_t flags);
    void free_page(void *page);
    void free_pages(void *page_start, size_t count);
    void *map_mmio(uint64_t phys_addr, size_t num_pages);
}