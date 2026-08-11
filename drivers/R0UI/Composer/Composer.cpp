#include "Composer.hpp"
#include "Members.hpp"
#include "lib/locks.hpp"

#include <TTY.hpp>
#include <LOG.hpp>
#include <HAL.hpp>
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
        if (next_free == 0) {
            return CMPSTR_STATE::NONE;
        }
        
        return cmpqueue[--next_free];
    }

    void handle_input(uint64_t k) {
        if (k == 'x') {
            Debug::krnl_print("R0UI", Debug::LOG_INFO, "I am %s (handle input)", HAL::CORE::get_core_data()->current_task->task_name.c_str());
        }
        Debug::krnl_print("R0UI", Debug::LOG_INFO, "You pressed %x", k);
    }

    void paint_init(uint32_t *ttybuffer, size_t x, size_t y) {
        HAL::MEM::FMEM::FastFill32(ttybuffer, 0xFF1F1F1F, x * y);
        HAL::SCREEN::add_damage(0, 0, x * 4, y);
        HAL::SCREEN::repaint();
    }

    size_t height, pitch, width;
    uint32_t *tty_buf{nullptr};
    void do_run_through() {
        static size_t do_redraw{0};

        asm volatile("mfence" ::: "memory");
        if (!linked_io) {
            if (++do_redraw > 20) {
                paint_init(tty_buf, height, width);
            }
            return;
        }
        
        volatile winpair *first = linked_io;

        do {
            lib::ScopedLock a(linklock);

            if (!first) // why lock here? Well for some reason, it crashes otherwise. Yeah.
                break; // And, it seems this works unreasonably well. The null check also happens to catch deletes? Not the above...
            first->ref->paint(tty_buf);
            first = first->next;
        } while (first != linked_io);

        return;
    }


    void worker1(uint32_t *tty_bbuf) {
        next_free = 0;
        cmpqueue[0] = CMPSTR_STATE::NONE;

        Debug::krnl_print("R0UI", Debug::LOG_INFO, "I am %s (default)", HAL::CORE::get_core_data()->current_task->task_name.c_str());

        append_queue(CMPSTR_STATE::INIT);

        (void)pitch; // "useful?"
        TTY::ScreenStructs::SCREEN_DATA b = TTY::get_scrdata();
        height = b.height;
        pitch = b.pitch;
        width = b.width;
        tty_buf = tty_bbuf;

        Debug::krnl_print("R0UI", Debug::LOG_INFO, "Running with visual address @ %x", tty_bbuf);

        w1_loop:
        switch (get_from_queue()) {
            case CMPSTR_STATE::INIT: {
                Debug::krnl_print("R0UI", Debug::LOG_INFO, "Performed a paint");
                paint_init(tty_bbuf, height, width);

                append_queue(CMPSTR_STATE::RUNNING);
                break;
            }
            case CMPSTR_STATE::HANDLE_KB: {
                break;
            }
            case CMPSTR_STATE::HANDLE_MOUSE: {
                break;
            }
            case CMPSTR_STATE::RUNNING: {
                do_run_through();
                append_queue(CMPSTR_STATE::RUNNING);
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