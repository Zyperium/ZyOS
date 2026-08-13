#pragma once

#include <stdint.h>
#include <stddef.h>

#include <SERVICES.hpp>
#include <lib/locks.hpp>
#include <lib/vec.hpp>
#include <lib/umap.hpp>
#include "r0ui_protocol.hpp"

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

        // Read-only sharing: lets another task watch this window's live pixel buffer without
        // ever getting a writable mapping to it. Returns a watcher-local pointer to a
        // WindowView (itself mapped read-only) describing the current buffer, or nullptr on
        // failure/self-watch. The WindowView is kept in sync by the kernel across resizes and
        // moves, so the watcher should always dereference view->pix_buf fresh rather than
        // caching the raw buffer pointer - it can move on every resize.
        WindowView *watch(Scheduler::Task *watcher_task);

        // Drops bookkeeping for a watcher without touching its page tables. Used when the
        // watcher itself is exiting (its address space is being torn down anyway, and by the
        // time this runs the Task pointer may already be on its way out).
        void remove_watcher(Scheduler::Task *watcher_task);

        static constexpr size_t DEFAULT_WINDOW_SIZE_W = 500;
        static constexpr size_t DEFAULT_WINDOW_SIZE_H = 350;
        static constexpr size_t WINDOWED_PADDING_AMOUNT = 10;

        WinControl *winref{nullptr};
        lib::string classname;
        uint64_t usr_pix{0};
        Rect factposn{};
        uint64_t owner{0}; 

    private:
        uint32_t *buffer{nullptr};
        char title[64]{0};

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

    extern lib::umap<Scheduler::Task *, lib::vec<Window *>> watched_resources;
}