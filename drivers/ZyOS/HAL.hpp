#pragma once
#include <stdint.h>
#include <stddef.h>

namespace HAL {
    namespace MEM {
        namespace PMM {
            void *alloc_page();

            void free_page(void *free_addr);

            void reference_page(void *page);
        }

        namespace VMM {
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
            void FreeProcessPages(uint64_t cr3);
        }

        namespace PMEM {
            void *alloc_page(uint64_t flags);
            void *alloc_pages(size_t count, uint64_t flags);

            void free_page(void *page);
            void free_pages(void *page_start, size_t count);
        }

        namespace KMEM {
            void* malloc(size_t size);
            void free(void* address);
            size_t get_size(void *ptr);

            void* calloc(size_t num, size_t size);
            void* realloc(void* ptr, size_t new_size);
        }
    }
}