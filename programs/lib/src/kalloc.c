#include <klibkrnl.h>
#include <kalloc.h>
#include <ksyscall.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define PAGE_SIZE       4096
#define ALIGNMENT       16
#define ALIGN(size)     (((size) + (ALIGNMENT - 1)) & ~(ALIGNMENT - 1))

#define PROT_READ_WRITE 0x3

typedef struct block_header {
    size_t size;            // Size of the usable payload space
    int is_free;            // 1 if available, 0 if allocated
    uint32_t _pad0;         // Align structure offset
    struct block_header *next;
    uint64_t _pad1;         // Force total sizeof(block_header_t) == 32
} block_header_t;

static block_header_t *g_heap_head = NULL;

void *malloc(size_t size) {
    if (size == 0) {
        return NULL;
    }

    size_t actual_size = ALIGN(size);

    block_header_t *curr = g_heap_head;
    block_header_t *last = NULL;

    while (curr) {
        if (curr->is_free && curr->size >= actual_size) {
            if (curr->size >= actual_size + sizeof(block_header_t) + ALIGNMENT) {
                block_header_t *new_block = (block_header_t *)((uint8_t *)(curr + 1) + actual_size);
                new_block->size = curr->size - actual_size - sizeof(block_header_t);
                new_block->is_free = 1;
                new_block->next = curr->next;

                curr->size = actual_size;
                curr->next = new_block;
            }

            curr->is_free = 0;
            return (void *)(curr + 1);
        }
        last = curr;
        curr = curr->next;
    }

    size_t total_needed = actual_size + sizeof(block_header_t);
    size_t mmap_size = (total_needed + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1); 

    uint64_t mmap_res = syscall(7, 0, mmap_size, PROT_READ_WRITE, 0, 0, 0);
    if (mmap_res == 0 || mmap_res == (uint64_t)-1) {
        return NULL;
    }

    block_header_t *block = (block_header_t *)mmap_res;
    block->size = mmap_size - sizeof(block_header_t);
    block->is_free = 0;
    block->next = NULL;

    if (!g_heap_head) {
        g_heap_head = block;
    } else if (last) {
        last->next = block;
    }

    if (block->size >= actual_size + sizeof(block_header_t) + ALIGNMENT) {
        block_header_t *remainder = (block_header_t *)((uint8_t *)(block + 1) + actual_size);
        remainder->size = block->size - actual_size - sizeof(block_header_t);
        remainder->is_free = 1;
        remainder->next = NULL;

        block->size = actual_size;
        block->next = remainder;
    }

    return (void *)(block + 1);
}

void free(void *ptr) {
    if (!ptr) {
        return;
    }

    block_header_t *block = ((block_header_t *)ptr) - 1;
    block->is_free = 1;

    block_header_t *curr = g_heap_head;
    while (curr && curr->next) {
        if (curr->is_free && curr->next->is_free) {
            if ((uint8_t *)(curr + 1) + curr->size == (uint8_t *)curr->next) {
                curr->size += sizeof(block_header_t) + curr->next->size;
                curr->next = curr->next->next;
                continue;
            }
        }
        curr = curr->next;
    }
}

void *calloc(size_t num, size_t size) {
    size_t total = num * size;
    void *ptr = malloc(total);
    if (ptr) {
        memset(ptr, 0, total);
    }
    return ptr;
}

void *realloc(void *ptr, size_t new_size) {
    if (!ptr) return malloc(new_size);
    if (new_size == 0) { free(ptr); return NULL; }

    block_header_t *header = ((block_header_t *)ptr) - 1;
    if (header->size >= new_size) {
        return ptr;
    }

    void *new_ptr = malloc(new_size);
    if (new_ptr) {
        memcpy(new_ptr, ptr, header->size);
        free(ptr);
    }
    return new_ptr;
}