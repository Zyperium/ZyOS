#include "Members.hpp"
#include "SERVICES.hpp"
#include "Composer.hpp"
#include "lib/locks.hpp"
#include "r0ui_protocol.hpp"

#include <LOG.hpp>
#include <HAL.hpp>
#include <lib/string.h>
#include <lib/regs.h>

using namespace HAL::MEM;

namespace R0UI {
    winpair *linked_io{nullptr};
    winpair *pinned_io{nullptr};
    lib::Spinlock linklock{};
    lib::umap<Scheduler::Task *, lib::vec<Window *>> watched_resources;

    Window::Window(Rect def) : factposn(def) {
        auto maxn = HAL::SCREEN::get_scrdata();

        if (factposn.width + WINDOWED_PADDING_AMOUNT > maxn.width)
            factposn.width = maxn.width - WINDOWED_PADDING_AMOUNT;

        if (factposn.height + WINDOWED_PADDING_AMOUNT > maxn.height)
            factposn.height = maxn.height - WINDOWED_PADDING_AMOUNT;

        uint32_t total_bytes = factposn.width * factposn.height * sizeof(uint32_t);
        uint32_t total_pages = (total_bytes + VMM::SIZE_OF_PAGE - 1) / VMM::SIZE_OF_PAGE;

        buffer = (uint32_t *)PMEM::alloc_pages(total_pages, 
            VMM::PTE_PRESENT | 
            VMM::PTE_WRITABLE | 
            VMM::PTE_WRITEBACK | 
            VMM::PTE_NX
        );

        winref = (WinControl *)PMEM::alloc_page(VMM::PTE_PRESENT | VMM::PTE_WRITABLE);
        memset(winref, 0, sizeof(WinControl));
        winref->height = factposn.height;
        winref->width = factposn.width;
        winref->x = factposn.x;
        winref->y = factposn.y;
        winref->scrnh = Composer::height;
        winref->scrnw = Composer::width;

        {
            Debug::krnl_print("R0UI", Debug::LOG_INFO, "Linking window");

            winpair *self_node{nullptr};

            if (!linked_io) {
                self_node = new winpair;
                self_node->ref = this;
                self_node->next = self_node;
                self_node->prev = self_node;
            } else {
                winpair *last = linked_io->prev;
                self_node = new winpair;

                self_node->ref = this;
                self_node->next = linked_io;
                self_node->prev = last;
                last->next = self_node;
                linked_io->prev = self_node;
            }

            linked_io = self_node;
        }
        
        has_deco.active = false;
        Debug::krnl_print("R0UI", Debug::LOG_INFO, "Damaging UI");
        Composer::add_damage(factposn.x, factposn.y, factposn.width, factposn.height);
    }

    uint32_t *Window::map_to(Scheduler::Task *pass_to) {
        uint32_t total_bytes = factposn.width * factposn.height * sizeof(uint32_t);
        uint32_t total_pages = (total_bytes + VMM::SIZE_OF_PAGE - 1) / VMM::SIZE_OF_PAGE;
        uint64_t write_at = pass_to->utask->usr_virt_mmap;
        pass_to->utask->usr_virt_mmap += total_pages * VMM::SIZE_OF_PAGE;

        for (auto i{0uz}; i < total_pages; ++i) {
            uintptr_t virt_ptr = reinterpret_cast<uintptr_t>(buffer) + (i * VMM::SIZE_OF_PAGE);
            uint64_t phys_addr = VMM::GetPhysicalAddress(read_cr3(), virt_ptr);
            VMM::map_page(
                (uint64_t *)(pass_to->cr3 + PMM::hhdm_offset), 
                write_at + (i * VMM::SIZE_OF_PAGE),
                phys_addr,
                VMM::PTE_PRESENT | VMM::PTE_WRITABLE | VMM::PTE_NX | 
                VMM::PTE_WRITEBACK | VMM::PTE_USER
            );
        }

        winref->usr_pix_buf = (uint32_t *)write_at;
        usr_pix = write_at;
        owner = (uint64_t)pass_to;

        write_at = pass_to->utask->usr_virt_mmap;
        pass_to->utask->usr_virt_mmap += VMM::SIZE_OF_PAGE;

        uintptr_t virt_ptr = reinterpret_cast<uintptr_t>(winref);
        uint64_t phys_addr = VMM::GetPhysicalAddress(read_cr3(), virt_ptr);
        VMM::map_page(
            (uint64_t *)(pass_to->cr3 + PMM::hhdm_offset), 
            write_at,
            phys_addr,
            VMM::PTE_PRESENT | VMM::PTE_WRITABLE | VMM::PTE_NX | 
            VMM::PTE_WRITEBACK | VMM::PTE_USER
        );

        return (uint32_t *)write_at;
    }

    void Window::set_pinned(bool pin) {
        if (pin == pinned) return;

        winpair *&src = pinned ? pinned_io : linked_io;

        winpair *tmp_io = src;
        if (tmp_io) {
            do {
                if (tmp_io->ref == this) break;
                tmp_io = tmp_io->next;
            } while (tmp_io != src);
        }

        if (!tmp_io || tmp_io->ref != this) {
            Debug::krnl_print("R0UI", Debug::LOG_WARN, "Window::set_pinned() couldn't find self in its own list?");
            return;
        }

        if (tmp_io->next == tmp_io) {
            src = nullptr;
        } else {
            if (tmp_io == src) {
                src = tmp_io->next;
            }
            tmp_io->prev->next = tmp_io->next;
            tmp_io->next->prev = tmp_io->prev;
        }

        pinned = pin;

        winpair *&dst = pinned ? pinned_io : linked_io;
        if (!dst) {
            tmp_io->next = tmp_io;
            tmp_io->prev = tmp_io;
        } else {
            winpair *last = dst->prev;
            tmp_io->next = dst;
            tmp_io->prev = last;
            last->next = tmp_io;
            dst->prev = tmp_io;
        }
        dst = tmp_io;

        Debug::krnl_print("R0UI", Debug::LOG_INFO, "%s window %s",
                           pinned ? "Pinned" : "Unpinned", classname.c_str());

        Composer::add_damage(factposn.x, factposn.y, factposn.width, factposn.height);
        Composer::force_redraw();
    }

    WindowView *Window::watch(Scheduler::Task *watcher_task) {
        if (!watcher_task || watcher_task == (Scheduler::Task *)owner) {
            return nullptr;
        }

        for (auto i{0uz}; i < watchers.size(); ++i) {
            if (watchers.data()[i].task == watcher_task) {
                return (WindowView *)watchers.data()[i].view_va;
            }
        }

        Watcher *slot{nullptr};
        for (auto i{0uz}; i < watchers.size(); ++i) {
            if (watchers.data()[i].task == nullptr) {
                slot = &watchers.data()[i];
                break;
            }
        }

        if (!slot) {
            Watcher fresh{};
            (void)watchers.push_back(fresh);
            slot = &watchers.data()[watchers.size() - 1];
        }

        WindowView *view = (WindowView *)PMEM::alloc_page(VMM::PTE_PRESENT | VMM::PTE_WRITABLE);
        memset(view, 0, sizeof(WindowView));

        uint64_t view_va = watcher_task->utask->usr_virt_mmap;
        watcher_task->utask->usr_virt_mmap += VMM::SIZE_OF_PAGE;

        uint64_t view_phys = VMM::GetPhysicalAddress(read_cr3(), (uintptr_t)view);
        VMM::map_page(
            (uint64_t *)(watcher_task->cr3 + PMM::hhdm_offset),
            view_va,
            view_phys,
            VMM::PTE_PRESENT | VMM::PTE_WRITEBACK | VMM::PTE_NX | VMM::PTE_USER
        );

        slot->task = watcher_task;
        slot->view = view;
        slot->view_va = view_va;
        slot->pix_va = 0;
        slot->mapped_pages = 0;

        map_watcher_pixels(*slot, watcher_task);

        view->pix_buf = (uint32_t *)slot->pix_va;
        view->width = factposn.width;
        view->height = factposn.height;
        view->x = factposn.x;
        view->y = factposn.y;
        view->scrnw = Composer::width;
        view->scrnh = Composer::height;
        view->valid = 1;
        view->generation = 1;

        (void)watched_resources[watcher_task].push_back(this);

        Debug::krnl_print("R0UI", Debug::LOG_INFO, "%s is now watching window %s",
                           watcher_task->task_name.c_str(), classname.c_str());

        return (WindowView *)view_va;
    }

    void Window::remove_watcher(Scheduler::Task *watcher_task) {
        for (auto i{0uz}; i < watchers.size(); ++i) {
            Watcher &w = watchers.data()[i];
            if (w.task != watcher_task) continue;

            if (w.view) {
                PMEM::free_page(w.view);
            }

            w = Watcher{};
            return;
        }
    }

    void Window::map_watcher_pixels(Watcher &w, Scheduler::Task *task) {
        uint32_t total_bytes = factposn.width * factposn.height * sizeof(uint32_t);
        uint32_t total_pages = (total_bytes + VMM::SIZE_OF_PAGE - 1) / VMM::SIZE_OF_PAGE;

        uint64_t old_va = w.pix_va;
        uint32_t old_pages = w.mapped_pages;

        uint64_t new_va;
        if (old_va && total_pages <= old_pages) {
            new_va = old_va;
        } else {
            new_va = task->utask->usr_virt_mmap;
            task->utask->usr_virt_mmap += total_pages * VMM::SIZE_OF_PAGE;
        }

        uint64_t cast_buf = (uint64_t)buffer;
        for (auto i{0uz}; i < total_pages; ++i) {
            uint64_t phys_addr = VMM::GetPhysicalAddress(read_cr3(), cast_buf + (i * VMM::SIZE_OF_PAGE));
            VMM::map_page(
                (uint64_t *)(task->cr3 + PMM::hhdm_offset),
                new_va + (i * VMM::SIZE_OF_PAGE),
                phys_addr,
                VMM::PTE_PRESENT | VMM::PTE_WRITEBACK | VMM::PTE_NX | VMM::PTE_USER
            );
        }

        if (new_va != old_va && old_va) {
            for (auto i{0uz}; i < old_pages; ++i) {
                VMM::unmap_page(
                    (uint64_t *)(task->cr3 + PMM::hhdm_offset),
                    old_va + (i * VMM::SIZE_OF_PAGE)
                );
            }
        } else if (new_va == old_va && total_pages < old_pages) {
            for (auto i{total_pages}; i < old_pages; ++i) {
                VMM::unmap_page(
                    (uint64_t *)(task->cr3 + PMM::hhdm_offset),
                    old_va + (i * VMM::SIZE_OF_PAGE)
                );
            }
        }

        w.pix_va = new_va;
        w.mapped_pages = total_pages;
    }

    void Window::unmap_watcher(Watcher &w) {
        if (!w.task || !w.pix_va) return;

        for (auto i{0uz}; i < w.mapped_pages; ++i) {
            VMM::unmap_page(
                (uint64_t *)(w.task->cr3 + PMM::hhdm_offset),
                w.pix_va + (i * VMM::SIZE_OF_PAGE)
            );
        }

        w.pix_va = 0;
        w.mapped_pages = 0;
    }

    void Window::remap_watchers() {
        for (auto i{0uz}; i < watchers.size(); ++i) {
            Watcher &w = watchers.data()[i];
            if (!w.task) continue;

            map_watcher_pixels(w, w.task);

            if (w.view) {
                w.view->pix_buf = (uint32_t *)w.pix_va;
                w.view->width = factposn.width;
                w.view->height = factposn.height;
                w.view->x = factposn.x;
                w.view->y = factposn.y;
                __builtin_ia32_sfence();
                w.view->generation++;
            }
        }
    }

    void Window::update_watcher_geometry() {
        for (auto i{0uz}; i < watchers.size(); ++i) {
            Watcher &w = watchers.data()[i];
            if (!w.task || !w.view) continue;

            w.view->x = factposn.x;
            w.view->y = factposn.y;
            __builtin_ia32_sfence();
            w.view->generation++;
        }
    }

    Window::~Window() {
        Composer::notify_window_destroyed(this);

        for (auto i{0uz}; i < watchers.size(); ++i) {
            Watcher &w = watchers.data()[i];
            if (!w.task) continue;

            if (w.view) {
                w.view->valid = 0;
                __builtin_ia32_sfence();
            }

            unmap_watcher(w);

            lib::vec<Window *> *watching = watched_resources.find(w.task);
            if (watching) {
                for (auto j{0uz}; j < watching->size(); ++j) {
                    if (watching->data()[j] == this) {
                        watching->data()[j] = nullptr;
                    }
                }
            }

            w.task = nullptr;
        }

        if (!buffer) {
            Debug::krnl_print("R0UI", Debug::LOG_WARN, "Bad window destruction?");
            return;
        }

        uint32_t total_bytes = factposn.width * factposn.height * sizeof(uint32_t);
        uint32_t total_pages = (total_bytes + VMM::SIZE_OF_PAGE - 1) / VMM::SIZE_OF_PAGE;
        PMEM::free_pages(buffer, total_pages);
        PMEM::free_page(winref);

        winpair *&home = pinned ? pinned_io : linked_io;

        if (!home) return;

        winpair *tmp_io = home;
        do {
            if (tmp_io->ref == this) break;
            tmp_io = tmp_io->next;
        } while (tmp_io != home);

        if (tmp_io->ref != this) {
            Debug::krnl_print("R0UI", Debug::LOG_INFO, "Unable to find self in list.");
            return;
        }

        if (tmp_io->next == tmp_io) {
            home = nullptr;
        } else {
            if (tmp_io == home) {
                home = tmp_io->next;
            }
            tmp_io->prev->next = tmp_io->next;
            tmp_io->next->prev = tmp_io->prev;
        }
        delete tmp_io;
    }

    void Window::move(int32_t nx, int32_t ny) {
        if ((uint32_t)nx > Composer::width || (uint32_t)ny > Composer::height) {
            Debug::krnl_print("R0UI", Debug::LOG_WARN, "Window::move() target out of bounds, ignoring.");
            return;
        }

        if (nx == factposn.x && ny == factposn.y) return;

        int32_t old_x = factposn.x;
        int32_t old_y = factposn.y;

        factposn.x = nx;
        factposn.y = ny;

        if (winref) {
            winref->x = nx;
            winref->y = ny;
        }

        Composer::add_damage(old_x, old_y, factposn.width, factposn.height);
        Composer::add_damage(nx, ny, factposn.width, factposn.height);

        update_watcher_geometry();
    }

    void Window::resize(uint32_t nwidth, uint32_t nheight) {
        if (nwidth == 0 || nheight == 0) return;

        if (nwidth > Composer::width) nwidth = Composer::width;
        if (nheight > Composer::height) nheight = Composer::height;
        if (nwidth == factposn.width && nheight == factposn.height) return;

        Scheduler::Task *ref = (Scheduler::Task *)owner;
        if (!ref) {
            Debug::krnl_print("R0UI", Debug::LOG_WARN, "Window::resize() called on a window with no owner task?");
            return;
        }

        uint32_t og_w = factposn.width;
        uint32_t og_h = factposn.height;

        factposn.width = nwidth;
        factposn.height = nheight;
        winref->width = nwidth;
        winref->height = nheight;

        realloc_pixel_buffer(ref, og_w, og_h);
        remap_watchers();

        uint32_t damage_w = (nwidth > og_w) ? nwidth : og_w;
        uint32_t damage_h = (nheight > og_h) ? nheight : og_h;
        Composer::add_damage(factposn.x, factposn.y, damage_w, damage_h);
    }

    void Window::realloc_pixel_buffer(Scheduler::Task *ref, uint32_t old_w, uint32_t old_h) {
        size_t og_buf_sz = (size_t)old_w * old_h * sizeof(uint32_t);
        size_t og_pages = (og_buf_sz + VMM::SIZE_OF_PAGE - 1) / VMM::SIZE_OF_PAGE;

        uint32_t total_bytes = factposn.width * factposn.height * sizeof(uint32_t);
        uint32_t total_pages = (total_bytes + VMM::SIZE_OF_PAGE - 1) / VMM::SIZE_OF_PAGE;
        uint64_t og_usr_pix = usr_pix;

        if (total_pages <= og_pages) {
            usr_pix = og_usr_pix;
        } else {
            usr_pix = ref->utask->usr_virt_mmap;
            ref->utask->usr_virt_mmap += total_pages * VMM::SIZE_OF_PAGE;
        }

        winref->usr_pix_buf = (uint32_t *)usr_pix;

        uint32_t *og_buf = buffer;
        buffer = (uint32_t *)PMEM::alloc_pages(
            total_pages,
            VMM::PTE_PRESENT | VMM::PTE_WRITABLE | VMM::PTE_WRITEBACK | VMM::PTE_NX
        );

        uint64_t cast_buf = (uint64_t)buffer;
        for (auto i{0uz}; i < total_pages; ++i) {
            uint64_t phys_addr = VMM::GetPhysicalAddress(read_cr3(), cast_buf + (i * VMM::SIZE_OF_PAGE));
            VMM::map_page(
                (uint64_t *)(ref->cr3 + PMM::hhdm_offset),
                usr_pix + (i * VMM::SIZE_OF_PAGE),
                phys_addr,
                VMM::PTE_PRESENT | VMM::PTE_WRITABLE | VMM::PTE_WRITEBACK | VMM::PTE_NX | VMM::PTE_USER
            );
        }

        uint32_t smaller_w = (factposn.width < old_w) ? factposn.width : old_w;
        uint32_t smaller_h = (factposn.height < old_h) ? factposn.height : old_h;

        for (auto i{0uz}; i < smaller_h; ++i) {
            FMEM::FastCopy(
                &buffer[i * factposn.width],
                &og_buf[i * old_w],
                smaller_w * sizeof(uint32_t)
            );
        }

        PMEM::free_pages(og_buf, og_pages);

        if (usr_pix != og_usr_pix) {
            for (auto i{0uz}; i < og_pages; ++i) {
                VMM::unmap_page(
                    (uint64_t *)(ref->cr3 + PMM::hhdm_offset),
                    og_usr_pix + (i * VMM::SIZE_OF_PAGE)
                );
            }
        } else if (total_pages < og_pages) {
            for (auto i{total_pages}; i < og_pages; ++i) {
                VMM::unmap_page(
                    (uint64_t *)(ref->cr3 + PMM::hhdm_offset),
                    usr_pix + (i * VMM::SIZE_OF_PAGE)
                );
            }
        }
    }

    void Window::readref(Scheduler::Task *ref) {
        int32_t proposed_x{winref->x}, proposed_y{winref->y};
        uint32_t proposed_w{winref->width}, proposed_h{winref->height};

        int32_t og_x = factposn.x;
        int32_t og_y = factposn.y;
        uint32_t og_w = factposn.width;
        uint32_t og_h = factposn.height;

        bool changed{false};
        bool size_changed{false};

        if ((uint32_t)proposed_x <= Composer::width)  { factposn.x = proposed_x; changed = true; }
        if ((uint32_t)proposed_y <= Composer::height) { factposn.y = proposed_y; changed = true; }

        if ((uint32_t)proposed_w <= Composer::width && proposed_w != og_w) {
            size_changed = changed = true;
            factposn.width = proposed_w;
        }
        if ((uint32_t)proposed_h <= Composer::height && proposed_h != og_h) {
            size_changed = changed = true;
            factposn.height = proposed_h;
        }

        winref->x = factposn.x;
        winref->y = factposn.y;
        winref->width = factposn.width;
        winref->height = factposn.height;

        bool og = has_deco.active;
        has_deco.active = winref->flags & WindowFlags::HasDecor;
        if (has_deco.active)
            has_deco.Realloc(winref->width);
        else if (og) 
            has_deco.Clean();

        if (size_changed) {
            realloc_pixel_buffer(ref, og_w, og_h);
            remap_watchers();
        } else if (changed) {
            update_watcher_geometry();
        }

        if (changed) {
            Composer::add_damage(og_x, og_y, og_w, og_h);
            Composer::add_damage(factposn.x, factposn.y, factposn.width, factposn.height);
        }
    }

    void Window::paint(uint32_t *screen) {
        uint32_t added_deco = (has_deco.active) ? DECO_HEIGHT : 0;

        int32_t clip_left   = (factposn.x < 0) ? 0 : factposn.x;
        int32_t clip_top    = (factposn.y < 0) ? 0 : factposn.y;
        int32_t clip_right  = (factposn.x + factposn.width > Composer::width) ? Composer::width : factposn.x + factposn.width;
        int32_t clip_bottom = (factposn.y + factposn.height > Composer::height) ? Composer::height : factposn.y + factposn.height;

        if (clip_left >= clip_right || clip_top >= clip_bottom)
            return;

        uint32_t copy_w = clip_right - clip_left;
        int32_t win_x = clip_left - factposn.x;

        for (int32_t y = clip_top; y < clip_bottom; ++y) {
            int32_t win_y = y - factposn.y;
            
            uint32_t dest_offset = clip_left + (y * Composer::width);
            uint32_t src_offset  = win_x + (win_y * factposn.width);

            HAL::MEM::FMEM::FastCopy(
                &screen[dest_offset],
                &buffer[src_offset],
                copy_w * sizeof(uint32_t)
            );
        }

        if (has_deco.active)
            has_deco.Paint(factposn.x, clip_top - added_deco, copy_w, screen);
    }

    bool Window::push_event(const Event &ev) {
        if (!winref) return false;

        uint32_t h = winref->events.head;
        uint32_t t = winref->events.tail;

        if (t >= EVENT_QUEUE_CAPACITY) t = 0;

        uint32_t next_head = (h + 1) % EVENT_QUEUE_CAPACITY;
        if (next_head == t) return false;

        winref->events.ring[h] = ev;
        __builtin_ia32_sfence();
        winref->events.head = next_head;
        
        return true;
    }
}