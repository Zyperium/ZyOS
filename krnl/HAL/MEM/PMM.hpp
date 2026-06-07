#pragma once
#include <limine.h>
#include <stdint.h>

namespace HAL::MEM::PMM {
    extern uint8_t *bitmap;
    extern uint16_t *ref_counts;
    extern uint64_t bitmap_size;
    extern uint64_t ref_counts_size;
    extern uint64_t total_memory;
    extern uint64_t used_memory;
    extern uint64_t hhdm_offset;

    void aquire_lock();
    void release_lock();

    void initialize(volatile limine_memmap_response *memmap_resp, volatile struct limine_hhdm_request *hhdm_resp);
    void *alloc_page();
    void free_page(void *free_addr);
    void reference_page(void *page);
}