#pragma once
#include <stdint.h>

namespace HAL::MEM::VMM {
    constexpr uint64_t PTE_PRESENT = 1;
    constexpr uint64_t PTE_WRITABLE = 2;
    constexpr uint64_t PTE_USER = 4;
    constexpr uint64_t PTE_WRITEBACK = 8;
    constexpr uint64_t PTE_CACHELESS = 16;
    constexpr uint64_t PTE_HUGE = 1ULL << 7;
    constexpr uint64_t PTE_NX = 1ULL << 63;
    constexpr uint64_t PTE_ADDR_MASK = 0x000FFFFFFFFFF000;

    uint64_t *get_next_level(uint64_t* current_table, uint64_t index, bool allocate, int level, uint64_t target_flags = 0);
    void map_page(uint64_t *pml4_root, uint64_t virt, uint64_t phys, uint64_t flags);
    void unmap_page(uint64_t *pml4_root, uint64_t virt);

    uint64_t GetPhysicalAddress(uint64_t cr3, uint64_t virtAddr);
    uint64_t CreateProcessPageTable(uint64_t kernel_pml4_phys);
    void FreeProcessPages(uint64_t cr3);
}