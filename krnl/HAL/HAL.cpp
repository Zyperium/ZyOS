#include "Library/string.h"
#include <HAL/HAL.hpp>
#include <HAL/IDT/IDT.hpp>
#include <HAL/MEM/MEM.hpp>
#include <HAL/MEM/KMEM.hpp>

#include <Library/debug.hpp>

#include <limine.h>


static constinit volatile limine_memmap_request memmap_request = {
    .id = LIMINE_MEMMAP_REQUEST_ID,
    .revision = 0,
    .response = nullptr
};

static constinit volatile struct limine_hhdm_request hhdm_request {
    .id = LIMINE_HHDM_REQUEST_ID,
    .revision = 0,
    .response = nullptr
};

static constinit volatile limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST_ID,
    .revision = 0,
    .response = nullptr
};


namespace HAL {
    void initialize() {
        Debug::krnl_print("HAL", Debug::LOG_INFO, "Initialize");
        MEM::initialize(&hhdm_request, memmap_request.response);

        IDT::initialize();
    }

    void *kmalloc(size_t size) {
        return MEM::KMEM::malloc(size);
    }

    void kfree(void *ptr) {
        return MEM::KMEM::free(ptr);
    }
}