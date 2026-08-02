#include "Composer.hpp"
#include <TTY.hpp>
#include <LOG.hpp>
#include <SERVICES.hpp>
#include <lib/string.h>

namespace R0UI::Composer {
    CMPSTR_STATE cmpqueue[(size_t)CMPSTR_STATE::SIZE]{CMPSTR_STATE::NONE};
    size_t next_free{0};

    bool already_queued(CMPSTR_STATE next_state) {
        for (auto i{0uz}; i < next_free; ++i) {
            if (next_state == cmpqueue[i])
                return true;
        }

        return false;
    }

    void append_queue(CMPSTR_STATE next_state) {
        if (already_queued(next_state)) {
            return;
        }

        if (next_free < (size_t)CMPSTR_STATE::SIZE) {
            cmpqueue[next_free] = next_state;
            ++next_free;
            return;
        }

        Debug::krnl_print("R0UI", Debug::LOG_WARN, "Queue is full, even though effects should stack. Memory corruption?");
    }

    CMPSTR_STATE get_from_queue() {
        --next_free;
        return cmpqueue[next_free];
    }

    void handle_input(uint64_t k) {
        (void)k;
    }

    void paint_init(uint32_t *ttybuffer, size_t x, size_t y) {
        memset32(ttybuffer, 0xFF00FF00, x * y);

    }

    void worker1(uint32_t *tty_bbuf) {
    append_queue(CMPSTR_STATE::INIT);

    size_t height, pitch;
    TTY::ScreenStructs::SCREEN_DATA b =  TTY::get_scrdata();
    height = b.height;
    pitch = b.pitch;

    Debug::krnl_print("CMPSR", Debug::LOG_INFO, "Running with visual address @ %x", tty_bbuf);

    w1_loop:
    switch (get_from_queue()) {
        case CMPSTR_STATE::INIT: {
            paint_init(tty_bbuf, height, pitch);
            break;
        }
        case CMPSTR_STATE::HANDLE_KB: {
            break;
        }
        case CMPSTR_STATE::HANDLE_MOUSE: {
            break;
        }
        case CMPSTR_STATE::RUNNING: {
            break;
        }
        case CMPSTR_STATE::SHUTDOWN: {
            break;
        }
        case CMPSTR_STATE::NONE: {
            Scheduler::Yield();
            break;
        }
        default:
            break;
    }
    
    goto w1_loop;
    }
}