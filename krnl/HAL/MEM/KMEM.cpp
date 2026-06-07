#include "Library/debug.hpp"
#include <Library/string.h>
#include <HAL/MEM/KMEM.hpp>
#include <HAL/MEM/VMM.hpp>
#include <HAL/MEM/PMM.hpp>

#include <Library/regs.h>

namespace HAL::MEM::KMEM {
    HeapSegmentHeader* first_segment;
    uint64_t* pml4_root;
    uint64_t current_heap_end;

    bool a_kmem_lock = false;
    uint64_t cur_rflags = 0;
    void aquire_lock() {
        uint64_t rflags = 0;
        asm volatile("pushfq; pop %0" : "=r"(rflags));
        asm volatile("cli");
        while (__atomic_test_and_set(&a_kmem_lock, __ATOMIC_ACQUIRE)) {
            asm volatile("pause");
        }

        cur_rflags = rflags;

        return;
    }

    void release_lock() {
        __atomic_clear(&a_kmem_lock, __ATOMIC_RELEASE);
        restore_rflags(cur_rflags);
        cur_rflags = 0;
        return;
    }

    void initialize(uint64_t* kernel_pml4, uint64_t heap_start_addr, size_t initial_pages) {
        Debug::krnl_print("KMEM", Debug::LOG_INFO, "Initialize");
        aquire_lock();

        pml4_root = kernel_pml4;

        uint64_t kernel_limit = heap_start_addr + (128ULL * 1024 * 1024 * 1024);
        for (uint64_t addr = heap_start_addr; addr < kernel_limit; addr += (2 * 1024 * 1024)) {
            VMM::map_page(pml4_root, addr, 0x0, 0);
        }

        current_heap_end = heap_start_addr;
        expand_heap(initial_pages * PAGE_SIZE);

        release_lock();
    }

    void expand_heap(size_t length) {
        align_up(length, PAGE_SIZE);

        size_t pages_needed = length / PAGE_SIZE;
        HeapSegmentHeader* start_seg = (HeapSegmentHeader*)current_heap_end;

        for (size_t i = 0; i < pages_needed; i++) {
            void* phys = PMM::alloc_page();
            VMM::map_page(pml4_root, current_heap_end, (uint64_t)phys, VMM::PTE_PRESENT | VMM::PTE_WRITABLE);
            current_heap_end += PAGE_SIZE;
        }

        HeapSegmentHeader* new_segment = start_seg;
        new_segment->length = length - sizeof(HeapSegmentHeader);
        new_segment->next = nullptr;
        new_segment->last = nullptr;
        new_segment->free = true;
        new_segment->magic = SEGMENT_MAGIC;

        if (first_segment == nullptr) {
            first_segment = new_segment;
        } else {
            HeapSegmentHeader* current = first_segment;
            while (current->next != nullptr) {
                current = current->next;
            }
            current->next = new_segment;
            new_segment->last = current;
        }
    }

    void expand_heap(size_t length, HeapSegmentHeader* last_known_segment) {
        align_up(length, PAGE_SIZE);

        size_t pages_needed = length / PAGE_SIZE;
        HeapSegmentHeader* start_seg = (HeapSegmentHeader*)current_heap_end;

        for (size_t i = 0; i < pages_needed; i++) {
            void* phys = PMM::alloc_page();
            VMM::map_page(pml4_root, current_heap_end, (uint64_t)phys, 3);
            current_heap_end += PAGE_SIZE;
        }

        HeapSegmentHeader* new_segment = start_seg;
        new_segment->length = length - sizeof(HeapSegmentHeader);
        new_segment->next = nullptr;
        new_segment->last = nullptr;
        new_segment->free = true;
        new_segment->magic = SEGMENT_MAGIC;

        if (first_segment == nullptr) {
            first_segment = new_segment;
        } else {
            HeapSegmentHeader* current = last_known_segment;
            while (current->next != nullptr) {
                current = current->next;
            }
            current->next = new_segment;
            new_segment->last = current;
        }
    }

    void *malloc(size_t size) {
        aquire_lock();

        if (size == 0) return nullptr;

        if (first_segment == nullptr) {
            release_lock();
            return nullptr;
        }

        align_up(size, KWORD);
        
        HeapSegmentHeader *current = first_segment;

        for (;;) {
            if (current->magic != SEGMENT_MAGIC) {
                release_lock();
                return nullptr;
            }

            if (current->free) {
                if (current->length >= size) {
                    current->free = false;
                    uint64_t payloadAddr = (uint64_t)current + sizeof(HeapSegmentHeader);
                    memset((void*)payloadAddr, 0xCC, size);
                    return (void*)payloadAddr;
                }
            }

            if (current->next == nullptr) {
                expand_heap(size + sizeof(HeapSegmentHeader), current);

                if (current->next == nullptr) {
                    release_lock();
                    return nullptr;
                }
            }

            current = current->next;

            if (current == nullptr) {
                release_lock();
                return nullptr;
            }
        }

        release_lock();
    }

    void combine_free_segments(HeapSegmentHeader* a, HeapSegmentHeader* b) {
        if (a == nullptr || b == nullptr) return;
        if (!a->free || !b->free) return;
        if (a->next != b) return;

        a->length += b->length + sizeof(HeapSegmentHeader);
        a->next = b->next;

        if (b->next != nullptr) {
            b->next->last = a;
        }
    }

    void free(void *addr) {
        if (!addr) return;

        aquire_lock();

        HeapSegmentHeader* header = (HeapSegmentHeader*)((uint64_t)addr - sizeof(HeapSegmentHeader));

        if (header->magic != SEGMENT_MAGIC) {
            return;
        }

        header->free = true;

        if (header->next != nullptr && header->next->free) {
            combine_free_segments(header, header->next);
        }

        if (header->last != nullptr && header->last->free) {
            combine_free_segments(header->last, header);
        }

        release_lock();
    }

    void* calloc(size_t num, size_t size) {
        void* ptr = malloc(num * size);
        if (ptr) memset(ptr, 0, num * size);
        return ptr;
    }

    void* realloc(void* ptr, size_t new_size) {
        if (!ptr) return malloc(new_size);
        if (new_size == 0) { free(ptr); return nullptr; }

        HeapSegmentHeader* header = (HeapSegmentHeader*)((uint64_t)ptr - sizeof(HeapSegmentHeader));
        if (header->length >= new_size) return ptr;

        void* new_ptr = malloc(new_size);
        if (new_ptr) {
            memcpy(new_ptr, ptr, header->length);
            free(ptr);
        }

        return new_ptr;
    }
}

void* operator new(size_t size) { return HAL::MEM::KMEM::malloc(size); }
void* operator new[](size_t size) { return HAL::MEM::KMEM::malloc(size); }
void operator delete(void* p) { HAL::MEM::KMEM::free(p); }
void operator delete[](void* p) { HAL::MEM::KMEM::free(p); }
void operator delete(void* p, size_t size) { (void)size; HAL::MEM::KMEM::free(p); }
void operator delete[](void* p, size_t size) { (void)size; HAL::MEM::KMEM::free(p); }