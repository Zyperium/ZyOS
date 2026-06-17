#include <linux/slab.h>
#include <HAL.hpp>

using namespace HAL::MEM;

extern "C" {
    void *kmalloc(size_t size, gfp_t flags) {
        (void)flags;
        void *ptr = KMEM::malloc(size);
        return ptr;
    }

    void kfree(void *ptr) {
        KMEM::free(ptr);
        return;
    }

    size_t ksize(void *ptr) {
        return KMEM::get_size(ptr);
    }

    void *krealloc(const void *ptr, size_t size, gfp_t flags) {
        (void)flags;

        return KMEM::realloc((void *)ptr, size);
    }

    void *kcalloc(size_t n, size_t size, gfp_t flags) {
        (void)flags;
        return KMEM::calloc(n, size);
    }
}