#include <Services/Input/Input.hpp>
#include <HAL/MEM/KMEM.hpp>

#define RING_BUF_MAX_SZ 128

namespace Input {
    char *ring_buf{nullptr};
    size_t rpointr{0};

    void add_kb(char nc) {
        if (!ring_buf)
            ring_buf = new char[RING_BUF_MAX_SZ];

        if (rpointr == RING_BUF_MAX_SZ - 1)
            return;

        ring_buf[++rpointr] = nc;
    }

    char pop_kb() {
        if (rpointr == 0)
            return '0';

        return ring_buf[--rpointr];
    }
}