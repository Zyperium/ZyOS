#include <HAL/MEM/PMM.hpp>
#include <Library/regs.h>
#include <Library/string.h>
#include <Library/debug.hpp>

namespace HAL::MEM::PMM {
    uint8_t* bitmap = nullptr;
    uint16_t *ref_counts = nullptr;
    uint64_t bitmap_size = 0;
    uint64_t ref_counts_size = 0;
    uint64_t total_memory = 0;
    uint64_t used_memory = 0;
    uint64_t hhdm_offset = 0;

    bool a_pmm_lock = false;
    uint64_t cur_rflags = 0;
    void aquire_lock() {
        uint64_t rflags = 0;
        asm volatile("pushfq; pop %0" : "=r"(rflags));
        asm volatile("cli");
        while (__atomic_test_and_set(&a_pmm_lock, __ATOMIC_ACQUIRE)) {
            asm volatile("pause");
        }

        cur_rflags = rflags;

        return;
    }

    bool is_page_used(uint64_t page_index) {
        uint8_t bytr = bitmap[page_index / 8];
        return (bytr >> (page_index % 8)) & 1;
    }

    void lock_page(uint64_t page_index) {
        if (!is_page_used(page_index)) {
            bitmap[page_index / 8] |= (1 << (page_index % 8));
            used_memory += PAGE_SIZE;
        }
    }

    void free_page_index(uint64_t page_index) {
        if (is_page_used(page_index)) {
            bitmap[page_index / 8] &= ~(1 << (page_index  % 8));
            used_memory -= PAGE_SIZE;
        }
    }

    void release_lock() {
        __atomic_clear(&a_pmm_lock, __ATOMIC_RELEASE);
        restore_rflags(cur_rflags);
        cur_rflags = 0;
        return;
    }

    void initialize(volatile struct limine_memmap_response *memmap_resp, volatile limine_hhdm_request *hhdm_resp) {
        Debug::krnl_print("PMM", Debug::LOG_INFO, "Initialize");
        if (hhdm_resp->response == nullptr) {
            for (;;) asm volatile("hlt");
        }

        hhdm_offset = hhdm_resp->response->offset;

        uint64_t highestaddr = 0;
        for (uint64_t i = 0; i < memmap_resp->entry_count; i++) {
            struct limine_memmap_entry *entry = memmap_resp->entries[i];

            if (entry->type == LIMINE_MEMMAP_USABLE ||
            entry->type == LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE) {
                uint64_t top = entry->base + entry->length;
                if (top > highestaddr) highestaddr = top; 
            }
        }

        total_memory = highestaddr;

        uint64_t total_pages = total_memory / PAGE_SIZE;
        bitmap_size = align_up(total_pages / 8, PAGE_SIZE);
        ref_counts_size = align_up(total_pages * sizeof(uint16_t), PAGE_SIZE);

        for (uint64_t i = 0; i < memmap_resp->entry_count; i++) {
            struct limine_memmap_entry *entry = memmap_resp->entries[i];

            if (entry->type == LIMINE_MEMMAP_USABLE && entry->length >= bitmap_size) {
                bitmap = (uint8_t*)(entry->base + hhdm_offset);

                memset(bitmap, 0xFF, bitmap_size);

                used_memory = total_memory;

                entry->base += bitmap_size;
                entry->length -= bitmap_size;
                break;
            }
        }

        for (uint64_t i = 0; i < memmap_resp->entry_count; i++) {
            struct limine_memmap_entry *entry = memmap_resp->entries[i];
            if (entry->type == LIMINE_MEMMAP_USABLE && entry->length >= ref_counts_size) {
                ref_counts = (uint16_t*)(entry->base + hhdm_offset);
                memset(ref_counts, 0, ref_counts_size);
                
                entry->base += ref_counts_size;
                entry->length -= ref_counts_size;
                break;
            }
        }

        for (uint64_t i = 0; i < memmap_resp->entry_count; i++) {
            struct limine_memmap_entry *entry = memmap_resp->entries[i];

            if (entry->type == LIMINE_MEMMAP_USABLE) {
                for (uint64_t addr = entry->base; addr < entry->base + entry->length; addr += PAGE_SIZE) {
                    free_page_index(addr / PAGE_SIZE);
                }
            }
        }

    }

    void *alloc_page() {
        aquire_lock();
        uint64_t total_pages = total_memory / PAGE_SIZE;

        for (uint64_t i = 0; i < total_pages; i++) {
            if (!is_page_used(i)) {
                lock_page(i);
                ref_counts[i] = 1;
                release_lock();
                return (void*)(i * PAGE_SIZE);
            }
        }

        release_lock();
        return nullptr;
    }

    void free_page(void *free_addr) {
        if ((uint64_t)free_addr > total_memory) {
            return;
        }

        aquire_lock();

        uint64_t ui_addr = (uint64_t)free_addr;
        uint64_t page_index = ui_addr / PAGE_SIZE;

        if (ref_counts[page_index] > 0) {
            ref_counts[page_index]--;
        }

        if (ref_counts[page_index] == 0) {
            free_page_index(page_index);
        }

        release_lock();
        return;
    }

    void reference_page(void *page) {
        aquire_lock();
        uint64_t ui_addr = (uint64_t)page;
        uint64_t page_index = ui_addr / PAGE_SIZE;
        
        ref_counts[page_index]++;
        release_lock();
        return;
    }
}