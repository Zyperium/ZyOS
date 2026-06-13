#pragma once
#include <stdint.h>
#include <stddef.h>

namespace HAL::MEM::KMEM {
    constexpr uint64_t SEGMENT_MAGIC = 0xDEADBEEF;
    constexpr uint64_t INITIAL_PAGES = 10;
    constexpr uint64_t DEFAULT_KMEM_START = 0xFFFF900000000000;
    constexpr uint8_t MEM_POISON_VALUE = 0xCC;

    extern uint64_t* pml4_root;

    struct HeapSegmentHeader {
        size_t length;
        HeapSegmentHeader* next;
        HeapSegmentHeader* last;
        bool free;
        uint64_t magic;
    };

    void initialize(uint64_t* kernel_pml4_root, uint64_t heap_start_addr, size_t initial_pages);

    void* malloc(size_t size);
    void free(void* address);

    void* calloc(size_t num, size_t size);
    void* realloc(void* ptr, size_t new_size);

    void expand_heap(size_t length);
    void combine_free_segments(HeapSegmentHeader* a, HeapSegmentHeader* b);
    void expand_heap(size_t length, HeapSegmentHeader* last_known_segment);

    void CheckHeapIntegrity();

    constexpr auto align_up(auto size, auto alignment) {
        using T = decltype(size);
        T align_m1 = static_cast<T>(alignment) - 1;
        return (size + align_m1) & ~align_m1;
    }
}

namespace std {
    enum class align_val_t : size_t {};
}

void *operator new(size_t size);
void *operator new(size_t size, std::align_val_t align);
void *operator new[](size_t size);
void operator delete(void* p);
void operator delete(void* p, uint64_t someval, std::align_val_t align);
void operator delete[](void* p);
void operator delete(void* p, size_t size);
void operator delete[](void* p, size_t size);
