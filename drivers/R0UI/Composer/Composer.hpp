#pragma once
#include <stdint.h>
#include <stddef.h>

namespace R0UI::Composer {
    extern size_t height, pitch, width;
    void worker1(uint32_t *tty_bbuf);

    void add_damage(int x, int y, int w, int h);
    void force_redraw();
    void handle_input(uint64_t k);

    struct IUPDATE {
        char key;
        bool pressed;

        int x, y;
        bool m1, m2;
        IUPDATE *next;
    };

    enum class CMPSTR_STATE : size_t {
        NONE,
        INIT,
        RUNNING,
        HANDLE_KB,
        HANDLE_MOUSE,
        SHUTDOWN,
        SIZE
    };

    static inline bool is_interrupt_enabled(void) {
        uint64_t rflags;
        
        // Push RFLAGS onto the stack, then pop it into a general-purpose register
        __asm__ __volatile__(
            "pushfq\n\t"
            "pop %0"
            : "=r"(rflags)
            :
            : "memory"
        );

        // The Interrupt Flag (IF) is at bit offset 9
        return (rflags & (1ULL << 9)) != 0;
    }
}