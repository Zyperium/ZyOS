#include "Members.hpp"
#include "SERVICES.hpp"
#include "Composer.hpp"
#include "lib/locks.hpp"

#include <LOG.hpp>
#include <HAL.hpp>
#include <lib/string.h>
#include <lib/regs.h>

using namespace HAL::MEM;

namespace R0UI {
    winpair *linked_io{nullptr};
    lib::Spinlock linklock{};

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
            // lib::ScopedLock n(linklock);
            if (!linked_io) {
                linked_io = new winpair;
                linked_io->ref = this;
                linked_io->next = linked_io;
                linked_io->prev = linked_io;
            } else {
                winpair *last = linked_io->prev;
                winpair *new_node = new winpair;

                new_node->ref = this;
                new_node->next = linked_io;
                new_node->prev = last;
                last->next = new_node;
                linked_io->prev = new_node;
            }
        }
        
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

    Window::~Window() {
        Composer::notify_window_destroyed(this);

        if (!buffer) {
            Debug::krnl_print("R0UI", Debug::LOG_WARN, "Bad window destruction?");
            return;
        }

        uint32_t total_bytes = factposn.width * factposn.height * sizeof(uint32_t);
        uint32_t total_pages = (total_bytes + VMM::SIZE_OF_PAGE - 1) / VMM::SIZE_OF_PAGE;
        PMEM::free_pages(buffer, total_pages);
        PMEM::free_page(winref);

        // lib::ScopedLock x(linklock);
        
        if (!linked_io) return;

        winpair *tmp_io = linked_io;
        do {
            if (tmp_io->ref == this) break;
            tmp_io = tmp_io->next;
        } while (tmp_io != linked_io);

        if (tmp_io->ref != this) {
            Debug::krnl_print("R0UI", Debug::LOG_INFO, "Unable to find self in list.");
            return;
        }

        if (tmp_io->next == tmp_io) {
            linked_io = nullptr;
        } else {
            if (tmp_io == linked_io) {
                linked_io = tmp_io->next;
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

        if (size_changed) {
            realloc_pixel_buffer(ref, og_w, og_h);
        }

        if (changed) {
            Composer::add_damage(og_x, og_y, og_w, og_h);
            Composer::add_damage(factposn.x, factposn.y, factposn.width, factposn.height);
        }
    }

    void Window::paint(uint32_t *screen) {
        int32_t clip_left   = (factposn.x < 0) ? 0 : factposn.x;
        int32_t clip_top    = (factposn.y < 0) ? 0 : factposn.y;
        int32_t clip_right  = (factposn.x + factposn.width > Composer::width) ? Composer::width : factposn.x + factposn.width;
        int32_t clip_bottom = (factposn.y + factposn.height > Composer::height) ? Composer::height : factposn.y + factposn.height;

        if (clip_left >= clip_right || clip_top >= clip_bottom)
            return;

        uint32_t copy_w = clip_right - clip_left;

        for (int32_t y = clip_top; y < clip_bottom; ++y) {
            int32_t win_y = y - factposn.y;
            int32_t win_x = clip_left - factposn.x;

            uint32_t dest_offset = clip_left + (y * Composer::width);
            uint32_t src_offset  = win_x + (win_y * factposn.width);

            memcpy(
                &screen[dest_offset],
                &buffer[src_offset],
                copy_w * sizeof(uint32_t)
            );
        }

        HAL::SCREEN::add_damage(clip_left, clip_top, copy_w, clip_bottom - clip_top);
        HAL::SCREEN::repaint();
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