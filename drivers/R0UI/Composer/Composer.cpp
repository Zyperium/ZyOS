#include "Composer.hpp"
#include "Members.hpp"
#include "lib/locks.hpp"

#include <LOG.hpp>
#include <HAL.hpp>
#include <SERVICES.hpp>
#include <lib/string.h>

namespace R0UI::Composer {
    size_t height = 0, pitch = 0, width = 0;
    uint32_t *tty_buf{nullptr};
    lib::Spinlock cmp_lock{};

    static volatile bool requires_redraw = true;
    static HardwareInputQueue input_queue{};

    static Rect current_damage{0, 0, 0, 0};

    static Window *focused_window{nullptr};
    static Point cursor_pos{0, 0};

    static bool push_hw_event(const HardwareInputEvent &ev) {
        uint32_t h = input_queue.head;
        uint32_t t = input_queue.tail;
        uint32_t next_head = (h + 1) % HW_INPUT_QUEUE_SIZE;

        if (next_head == t) return false;

        input_queue.ring[h] = ev;
        __builtin_ia32_sfence();
        input_queue.head = next_head;
        return true;
    }

    void handle_kb_input(uint64_t k, bool pressed) {
        HardwareInputEvent ev{};
        ev.type = HardwareInputEvent::Type::Keyboard;
        ev.data.kb.keycode = k;
        ev.data.kb.pressed = pressed;
        push_hw_event(ev);
    }

    void handle_mouse_input(int32_t rel_x, int32_t rel_y, bool m1, bool m2) {
        HardwareInputEvent ev{};
        ev.type = HardwareInputEvent::Type::Mouse;
        ev.data.mouse.rel_x = rel_x;
        ev.data.mouse.rel_y = rel_y;
        ev.data.mouse.m1 = m1;
        ev.data.mouse.m2 = m2;
        push_hw_event(ev);
    }

    void add_damage(int32_t x, int32_t y, uint32_t w, uint32_t h) {
        if (w == 0 || h == 0) return;

        lib::ScopedLock lock(cmp_lock);

        if (current_damage.width == 0 || current_damage.height == 0) {
            current_damage = {x, y, w, h};
        } else {
            int32_t left   = (x < current_damage.x) ? x : current_damage.x;
            int32_t top    = (y < current_damage.y) ? y : current_damage.y;
            int32_t right  = ((x + (int32_t)w) > (current_damage.x + (int32_t)current_damage.width))
                                ? (x + (int32_t)w) : (current_damage.x + (int32_t)current_damage.width);
            int32_t bottom = ((y + (int32_t)h) > (current_damage.y + (int32_t)current_damage.height))
                                ? (y + (int32_t)h) : (current_damage.y + (int32_t)current_damage.height);

            current_damage.x = left;
            current_damage.y = top;
            current_damage.width = right - left;
            current_damage.height = bottom - top;
        }

        requires_redraw = true;
    }

    void force_redraw() {
        requires_redraw = true;
    }

    Window *get_focused_window() {
        lib::ScopedLock lock(cmp_lock);
        return focused_window;
    }

    void notify_window_destroyed(Window *w) {
        lib::ScopedLock lock(cmp_lock);
        if (focused_window == w) {
            focused_window = nullptr;
            requires_redraw = true;
        }
    }

    static void paint_init(uint32_t *ttybuffer, size_t w, size_t h) {
        HAL::MEM::FMEM::FastFill32(ttybuffer, 0xFF1F1F1F, w * h);
        HAL::SCREEN::add_damage(0, 0, w, h);
        HAL::SCREEN::repaint();
    }

    void do_run_through() {
        if (!requires_redraw) return;

        asm volatile("mfence" ::: "memory");

        lib::ScopedLock lock(linklock);

        if (!linked_io) {
            paint_init(tty_buf, width, height);
            requires_redraw = false;
            current_damage = {0, 0, 0, 0};
            return;
        }

        HAL::MEM::FMEM::FastFill32(tty_buf, 0xFF1F1F1F, width * height);

        winpair *node = linked_io->prev; 
        if (node) {
            do {
                if (node->ref) {
                    node->ref->paint(tty_buf);
                }
                node = node->prev;
            } while (node != linked_io->prev && node != nullptr);
        }

        requires_redraw = false;

        lib::ScopedLock dmg_lock(cmp_lock);
        current_damage = {0, 0, 0, 0};
    }

    static Window *hit_test(int32_t x, int32_t y) {
        lib::ScopedLock lock(linklock);
        if (!linked_io) return nullptr;

        Window *hit{nullptr};
        winpair *node = linked_io->prev;
        do {
            if (node->ref && node->ref->factposn.contains({x, y})) {
                hit = node->ref;
            }
            node = node->prev;
        } while (node != linked_io->prev && node != nullptr);

        return hit;
    }

    static void dispatch_keyboard(const HardwareInputEvent &ev) {
        if (!focused_window) return;

        Event out{};
        out.type = ev.data.kb.pressed ? EventType::KeyDown : EventType::KeyUp;
        out.data.key.keycode = (uint32_t)ev.data.kb.keycode;
        out.data.key.modifiers = 0; // TODO: track shift/ctrl/alt once a keymap layer exists
        out.data.key.pressed = ev.data.kb.pressed ? 1 : 0;

        focused_window->push_event(out);
    }

    static void dispatch_mouse(const HardwareInputEvent &ev) {
        cursor_pos.x += ev.data.mouse.rel_x;
        cursor_pos.y += ev.data.mouse.rel_y;

        if (cursor_pos.x < 0) cursor_pos.x = 0;
        if (cursor_pos.y < 0) cursor_pos.y = 0;
        if ((uint32_t)cursor_pos.x >= (uint32_t)width)  cursor_pos.x = (int32_t)width - 1;
        if ((uint32_t)cursor_pos.y >= (uint32_t)height) cursor_pos.y = (int32_t)height - 1;

        static bool prev_m1{false};
        bool m1_edge_down = ev.data.mouse.m1 && !prev_m1;
        bool m1_edge_up   = !ev.data.mouse.m1 && prev_m1;
        prev_m1 = ev.data.mouse.m1;

        Window *target = hit_test(cursor_pos.x, cursor_pos.y);

        // Click-to-focus: only steal focus on a fresh press, never mid-drag.
        if (m1_edge_down && target != focused_window) {
            focused_window = target;
            force_redraw(); // so focus decor/highlight can update later
        }

        if (!focused_window) return;

        Event out{};
        out.data.mouse.rel_x = ev.data.mouse.rel_x;
        out.data.mouse.rel_y = ev.data.mouse.rel_y;
        out.data.mouse.buttons = (ev.data.mouse.m1 ? 0x1 : 0) | (ev.data.mouse.m2 ? 0x2 : 0);

        out.type = EventType::MouseMove;
        focused_window->push_event(out);

        if (m1_edge_down) {
            Event down = out;
            down.type = EventType::MouseDown;
            focused_window->push_event(down);
        } else if (m1_edge_up) {
            Event up = out;
            up.type = EventType::MouseUp;
            focused_window->push_event(up);
        }

        if (focused_window->winref) {
            focused_window->winref->mouse_pos.x = cursor_pos.x - focused_window->factposn.x;
            focused_window->winref->mouse_pos.y = cursor_pos.y - focused_window->factposn.y;
        }
    }

    static void process_inputs() {
        uint32_t h = input_queue.head;
        uint32_t t = input_queue.tail;

        while (t != h) {
            HardwareInputEvent ev = input_queue.ring[t];
            __builtin_ia32_lfence();

            if (ev.type == HardwareInputEvent::Type::Keyboard) {
                Debug::krnl_print("R0UI", Debug::LOG_INFO, "Key: %x (Pressed: %d)", 
                                  ev.data.kb.keycode, ev.data.kb.pressed);
                dispatch_keyboard(ev);
            } else if (ev.type == HardwareInputEvent::Type::Mouse) {
                dispatch_mouse(ev);
            }

            t = (t + 1) % HW_INPUT_QUEUE_SIZE;
        }
        input_queue.tail = t;
    }

    void worker1(uint32_t *tty_bbuf) {
        Debug::krnl_print("R0UI", Debug::LOG_INFO, "I am %s (Composer)", HAL::CORE::get_core_data()->current_task->task_name.c_str());

        auto b = HAL::SCREEN::get_scrdata();
        height = b.height;
        pitch = b.pitch;
        width = b.width;
        tty_buf = tty_bbuf;

        Debug::krnl_print("R0UI", Debug::LOG_INFO, "Composer running with visual address @ %x", tty_bbuf);

        paint_init(tty_buf, width, height);

        for (;;) {
            bool performed_work = false;

            if (input_queue.head != input_queue.tail) {
                process_inputs();
                performed_work = true;
            }

            if (requires_redraw) {
                do_run_through();
                performed_work = true;
            }

            if (!performed_work) {
                Scheduler::Yield();
            }
        }
    }
}