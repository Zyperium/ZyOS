#include "Composer.hpp"
#include "Members.hpp"
#include "lib/locks.hpp"

#include <LOG.hpp>
#include <HAL.hpp>
#include <SERVICES.hpp>
#include <lib/string.h>
#include <lib/regs.h>

using namespace HAL::MEM;

namespace R0UI::Composer {
    size_t height = 0, pitch = 0, width = 0;
    uint32_t *tty_buf{nullptr};
    lib::Spinlock cmp_lock{};

    static volatile bool requires_redraw = true;
    static HardwareInputQueue input_queue{};

    static Rect current_damage{0, 0, 0, 0};

    static Window *focused_window{nullptr};
    static Point cursor_pos{0, 0};

    static uint32_t *wallpaper_buffer{nullptr};
    static uint64_t wallpaper_owner{0};
    static uint64_t wallpaper_usr_va{0};
    static uint32_t wallpaper_pages{0};

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

    uint32_t *request_wallpaper(Scheduler::Task *owner) {
        lib::ScopedLock lock(cmp_lock);

        if (wallpaper_buffer) {
            if (wallpaper_owner == (uint64_t)owner) {
                return (uint32_t *)wallpaper_usr_va;
            }
            return nullptr;
        }

        uint32_t total_bytes = (uint32_t)(width * height * sizeof(uint32_t));
        uint32_t total_pages = (total_bytes + VMM::SIZE_OF_PAGE - 1) / VMM::SIZE_OF_PAGE;

        wallpaper_buffer = (uint32_t *)PMEM::alloc_pages(total_pages,
            VMM::PTE_PRESENT | VMM::PTE_WRITABLE | VMM::PTE_WRITEBACK | VMM::PTE_NX);

        FMEM::FastFill32(wallpaper_buffer, 0xFF1F1F1F, width * height);

        uint64_t write_at = owner->utask->usr_virt_mmap;
        owner->utask->usr_virt_mmap += total_pages * VMM::SIZE_OF_PAGE;

        uint64_t cast_buf = (uint64_t)wallpaper_buffer;
        for (auto i{0uz}; i < total_pages; ++i) {
            uint64_t phys_addr = VMM::GetPhysicalAddress(read_cr3(), cast_buf + (i * VMM::SIZE_OF_PAGE));
            VMM::map_page(
                (uint64_t *)(owner->cr3 + PMM::hhdm_offset),
                write_at + (i * VMM::SIZE_OF_PAGE),
                phys_addr,
                VMM::PTE_PRESENT | VMM::PTE_WRITABLE | VMM::PTE_NX | VMM::PTE_WRITEBACK | VMM::PTE_USER
            );
        }

        wallpaper_owner = (uint64_t)owner;
        wallpaper_usr_va = write_at;
        wallpaper_pages = total_pages;

        force_redraw();

        return (uint32_t *)write_at;
    }

    void release_wallpaper(Scheduler::Task *owner) {
        lib::ScopedLock lock(cmp_lock);

        if (!wallpaper_buffer || wallpaper_owner != (uint64_t)owner) return;

        PMEM::free_pages(wallpaper_buffer, wallpaper_pages);

        wallpaper_buffer = nullptr;
        wallpaper_owner = 0;
        wallpaper_usr_va = 0;
        wallpaper_pages = 0;

        force_redraw();
    }

    static void paint_init(uint32_t *ttybuffer, size_t w, size_t h) {
        FMEM::FastFill32(ttybuffer, 0xFF1F1F1F, w * h);
        HAL::SCREEN::add_damage(0, 0, w, h);
        HAL::SCREEN::repaint();
        Debug::krnl_print("R0UI", Debug::LOG_INFO, "Performed init paint");
    }

    static void paint_list(winpair *head, uint32_t *ttybuffer) {
        if (!head) return;

        winpair *node = head->prev;
        do {
            if (node->ref) {
                node->ref->paint(ttybuffer);
            }
            node = node->prev;
        } while (node != head->prev && node != nullptr);
    }

    static constexpr int32_t CURSOR_W = 15;
    static constexpr int32_t CURSOR_H = 23;

    static const uint8_t cursor_sprite[CURSOR_H][CURSOR_W] = {
        {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {1,1,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {1,2,1,0,0,0,0,0,0,0,0,0,0,0,0},
        {1,2,2,1,0,0,0,0,0,0,0,0,0,0,0},
        {1,2,2,2,1,0,0,0,0,0,0,0,0,0,0},
        {1,2,2,2,2,1,0,0,0,0,0,0,0,0,0},
        {1,2,2,2,2,2,1,0,0,0,0,0,0,0,0},
        {1,2,2,2,2,2,2,1,0,0,0,0,0,0,0},
        {1,2,2,2,2,2,2,2,1,0,0,0,0,0,0},
        {1,2,2,2,2,2,2,2,2,1,0,0,0,0,0},
        {1,2,2,2,2,2,2,2,2,2,1,0,0,0,0},
        {1,2,2,2,2,2,2,2,2,2,2,1,0,0,0},
        {1,2,2,2,2,2,2,2,2,2,2,2,1,0,0},
        {1,2,2,2,2,2,2,2,2,2,2,2,2,1,0},
        {1,2,2,2,2,2,1,1,1,1,1,1,1,1,1},
        {1,2,2,2,2,1,0,0,0,0,0,0,0,0,0},
        {1,2,2,2,1,0,0,0,0,0,0,0,0,0,0},
        {1,2,2,1,0,0,0,0,0,0,0,0,0,0,0},
        {1,2,1,0,0,0,0,0,0,0,0,0,0,0,0},
        {1,1,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}
    };

    static void draw_cursor(uint32_t *screen) {
        int32_t cx = cursor_pos.x;
        int32_t cy = cursor_pos.y;

        for (int32_t row = 0; row < CURSOR_H; ++row) {
            int32_t y = cy + row;
            if (y < 0 || (uint32_t)y >= height) continue;

            for (int32_t col = 0; col < CURSOR_W; ++col) {
                uint8_t px = cursor_sprite[row][col];
                if (px == 0) continue; // transparent, leave whatever was painted underneath

                int32_t x = cx + col;
                if (x < 0 || (uint32_t)x >= width) continue;

                screen[(uint32_t)y * width + (uint32_t)x] = (px == 1) ? 0xFFFFFFFF : 0xFF000000;
            }
        }
    }

    void do_run_through() {
        if (!requires_redraw) return;

        asm volatile("mfence" ::: "memory");
        asm volatile("sfence" ::: "memory");
        asm volatile("lfence" ::: "memory");

        lib::ScopedLock lock(linklock);

        if (wallpaper_buffer) {
            FMEM::FastCopy(tty_buf, wallpaper_buffer, width * height * sizeof(uint32_t));
        } else {
            FMEM::FastFill32(tty_buf, 0xFF1F1F1F, width * height);
        }

        paint_list(linked_io, tty_buf);
        paint_list(pinned_io, tty_buf);

        draw_cursor(tty_buf);

        HAL::SCREEN::add_damage(0, 0, width, height);
        HAL::SCREEN::repaint();

        requires_redraw = false;

        lib::ScopedLock dmg_lock(cmp_lock);
        current_damage = {0, 0, 0, 0};
    }

    static Window *hit_test_list(winpair *head, int32_t x, int32_t y, Window *hit) {
        if (!head) return hit;

        winpair *node = head->prev;
        do {
            if (node->ref && node->ref->factposn.contains({x, y})) {
                hit = node->ref;
            }
            node = node->prev;
        } while (node != head->prev && node != nullptr);

        return hit;
    }

    static Window *hit_test(int32_t x, int32_t y) {
        lib::ScopedLock lock(linklock);

        Window *hit = hit_test_list(linked_io, x, y, nullptr);
        hit = hit_test_list(pinned_io, x, y, hit);
        return hit;
    }

    static Window *pinned_hit_test(int32_t x, int32_t y) {
        lib::ScopedLock lock(linklock);
        return hit_test_list(pinned_io, x, y, nullptr);
    }

    static void dispatch_keyboard(const HardwareInputEvent &ev) {
        if (!focused_window) return;

        Event out{};
        out.type = ev.data.kb.pressed ? EventType::KeyDown : EventType::KeyUp;
        out.data.key.keycode = (uint32_t)ev.data.kb.keycode;
        out.data.key.modifiers = 0;
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

        if (ev.data.mouse.rel_x != 0 || ev.data.mouse.rel_y != 0) {
            force_redraw();
        }

        Window *target = hit_test(cursor_pos.x, cursor_pos.y);

        if (m1_edge_down && target != focused_window) {
            focused_window = target;
            force_redraw();
        }

        Event out{};
        out.data.mouse.rel_x = ev.data.mouse.rel_x;
        out.data.mouse.rel_y = ev.data.mouse.rel_y;
        out.data.mouse.buttons = (ev.data.mouse.m1 ? 0x1 : 0) | (ev.data.mouse.m2 ? 0x2 : 0);

        auto deliver = [&](Window *w) {
            if (!w) return;

            Event mv = out;
            mv.type = EventType::MouseMove;
            w->push_event(mv);

            if (m1_edge_down) {
                Event down = out;
                down.type = EventType::MouseDown;
                w->push_event(down);
            } else if (m1_edge_up) {
                Event up = out;
                up.type = EventType::MouseUp;
                w->push_event(up);
            }

            if (w->winref) {
                w->winref->mouse_pos.x = cursor_pos.x - w->factposn.x;
                w->winref->mouse_pos.y = cursor_pos.y - w->factposn.y;
            }
        };

        // Pinned windows (taskbar, dock, etc.) always get hover/click input for
        // whatever's under the cursor, regardless of who currently has focus.
        Window *pinned_target = pinned_hit_test(cursor_pos.x, cursor_pos.y);
        deliver(pinned_target);

        // The focused window still gets its own stream too, unless it's the
        // same pinned window we already delivered to above.
        if (focused_window && focused_window != pinned_target) {
            deliver(focused_window);
        }
    }

    static void process_inputs() {
        uint32_t h = input_queue.head;
        uint32_t t = input_queue.tail;

        while (t != h) {
            HardwareInputEvent ev = input_queue.ring[t];
            __builtin_ia32_lfence();

            if (ev.type == HardwareInputEvent::Type::Keyboard) {
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

        Input::reg_kb_cb([](char c){
            handle_kb_input(c, true);
        });

        Input::reg_ms_cb([](const Input::MousePos mp){
            handle_mouse_input(mp.delta_x, mp.delta_y, mp.buttons & 0x1, mp.buttons & 0x2);
        });

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