#pragma once

#include <stdint.h>
#include <stddef.h>
#include <lib/locks.hpp>
#include <SERVICES.hpp>

namespace R0UI {
    class Window;
}

namespace R0UI::Composer {
    extern size_t height, pitch, width;
    extern lib::Spinlock cmp_lock;

    struct HardwareInputEvent {
        enum class Type : uint8_t {
            None = 0,
            Keyboard,
            Mouse
        } type;

        union {
            struct {
                uint64_t keycode;
                bool pressed;
            } kb;
            struct {
                int32_t rel_x, rel_y;
                bool m1, m2;
            } mouse;
        } data;
    };

    constexpr size_t HW_INPUT_QUEUE_SIZE = 256;
    struct HardwareInputQueue {
        volatile uint32_t head;
        volatile uint32_t tail;
        HardwareInputEvent ring[HW_INPUT_QUEUE_SIZE];
    };

    void worker1(uint32_t *tty_bbuf);
    
    void add_damage(int32_t x, int32_t y, uint32_t w, uint32_t h);
    void force_redraw();

    void handle_kb_input(uint64_t k, bool pressed);
    void handle_mouse_input(int32_t rel_x, int32_t rel_y, bool m1, bool m2);
    
    void do_run_through();

    Window *get_focused_window();

    void notify_window_destroyed(Window *w);

    uint32_t *request_wallpaper(Scheduler::Task *owner);
    void release_wallpaper(Scheduler::Task *owner);

    static inline bool is_interrupt_enabled(void) {
        uint64_t rflags;
        __asm__ __volatile__(
            "pushfq\n\t"
            "pop %0"
            : "=r"(rflags)
            :
            : "memory"
        );
        return (rflags & (1ULL << 9)) != 0;
    }
}