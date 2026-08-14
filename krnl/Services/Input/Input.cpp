#include <Services/Scheduler/Scheduler.hpp>
#include <Library/io.hpp>
#include <Services/Input/Input.hpp>
#include <HAL/MEM/KMEM.hpp>

#define RING_BUF_MAX_SZ 128

namespace Input {
    char *ring_buf{nullptr};
    size_t rpointr{0};
    void (*callback)(char c){nullptr};
    void (*mscallback)(const MousePos ref){nullptr};

    void add_kb(char nc) {
        if (callback)
            callback(nc);
        return;
    }

    void reg_kb_cb(void (*xz)(char c)) {
        if (!callback)
        callback = xz;
    }

    void add_mouse(const MousePos ref) {
        if (!mscallback)
            return;
        mscallback(ref);
    }

    void reg_ms_cb(void (*xmscallback)(const MousePos ref)) {
        if (!mscallback)
        mscallback = xmscallback;
    }
}