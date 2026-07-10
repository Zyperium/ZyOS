#include "Composer.hpp"
#include <TTY.hpp>
#include <lib/string.h>

namespace R0UI::Composer {
    volatile bool iup_lock = false;
    volatile IUPDATE *iring;

    IUPDATE grab_current() {
        if (!iring) return {};
        
        IUPDATE real;
        memcpy(&real, (void *)iring, sizeof(IUPDATE));

        volatile IUPDATE *next = iring->next;
        delete iring;
        iring = next;

        return real;
    }

    void handle_input(uint64_t k) {
        (void)k;
    }

    void worker1() {

    }
    
    void worker2(uint32_t *tty_bbuf) {
        (void)tty_bbuf;
    }
}