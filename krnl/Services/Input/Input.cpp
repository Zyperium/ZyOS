#include <Services/Input/Input.hpp>
#include <HAL/MEM/KMEM.hpp>

#define RING_BUF_MAX_SZ 128

namespace Input {
    char *ring_buf{nullptr};
    size_t rpointr{0};
    void (*callback)(char c){nullptr};

    void add_kb(char nc) {
        if (callback)
            callback(nc);
        return;
    }

    void reg_kb_cb(void (*xz)(char c)) {
        if (!callback)
        callback = xz;
    }
}