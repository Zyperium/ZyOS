#pragma once

#include <stdint.h>
#include <stddef.h>

#include <SERVICES.hpp>
#include <lib/locks.hpp>
#include <lib/vec.hpp>
#include <lib/umap.hpp>
#include "r0ui_protocol.hpp"
#include "Deco.hpp"

namespace R0UI {
    class Window {
    public:
        Window(Rect def);
        ~Window();

        void move(int32_t nx, int32_t ny);
        void resize(uint32_t nwidth, uint32_t nheight);
        void paint(uint32_t *screen);
        void readref(Scheduler::Task *ref);

        uint32_t *map_to(Scheduler::Task *pass_to);
        bool push_event(const Event &ev);

        WindowView *watch(Scheduler::Task *watcher_task);
        void remove_watcher(Scheduler::Task *watcher_task);

        void set_pinned(bool pin);
        [[nodiscard]] bool is_pinned() const { return pinned; }

        static constexpr size_t DEFAULT_WINDOW_SIZE_W = 500;
        static constexpr size_t DEFAULT_WINDOW_SIZE_H = 350;
        static constexpr size_t WINDOWED_PADDING_AMOUNT = 10;

        WinControl *winref{nullptr};
        DecoRoot has_deco;
        lib::string classname;
        uint64_t usr_pix{0};
        Rect factposn{};
        uint64_t owner{0}; 

    private:
        uint32_t *buffer{nullptr};
        char title[64]{0};
        bool pinned{false};

        struct Watcher {
            Scheduler::Task *task{nullptr};
            WindowView *view{nullptr};
            uint64_t view_va{0};
            uint64_t pix_va{0};
            uint32_t mapped_pages{0};
        };

        lib::vec<Watcher> watchers;

        void realloc_pixel_buffer(Scheduler::Task *ref, uint32_t old_w, uint32_t old_h);

        void map_watcher_pixels(Watcher &w, Scheduler::Task *task);
        void unmap_watcher(Watcher &w);
        void remap_watchers();
        void update_watcher_geometry();
    };

    struct winpair {
        Window *ref;
        winpair *next;
        winpair *prev;
    };

    extern lib::Spinlock linklock;
    extern winpair *linked_io;
    extern winpair *pinned_io;

    extern lib::umap<Scheduler::Task *, lib::vec<Window *>> watched_resources;
}