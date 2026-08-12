#include "Members.hpp"
#include "SERVICES.hpp"
#include "Composer.hpp"
#include "lib/locks.hpp"

#include <LOG.hpp>
#include <HAL.hpp>
#include <TTY.hpp>
#include <lib/string.h>
#include <lib/regs.h>

using namespace HAL::MEM;

namespace R0UI {
    volatile winpair *linked_io{nullptr};
    lib::Spinlock linklock{};

    Window::Window(P2D def) : factposn(def) {
        auto maxn = TTY::get_scrdata();

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

        lib::ScopedLock n(linklock);
        if (!linked_io) {
            linked_io = new winpair;
            linked_io->ref = this;
            linked_io->next = linked_io;
            linked_io->prev = linked_io;
        }
        else {
            volatile winpair *last = linked_io->prev;

            linked_io->prev = new winpair;
            linked_io->prev->next = linked_io;
            linked_io->prev->prev = last;
            last->next = linked_io->prev;
            linked_io->prev->ref = this;
        }
        
        return;
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
                (uint64_t)write_at + (i * VMM::SIZE_OF_PAGE),
                phys_addr,
                VMM::PTE_PRESENT | 
                VMM::PTE_WRITABLE | 
                VMM::PTE_NX | 
                VMM::PTE_WRITEBACK | 
                VMM::PTE_USER
            );
        }

        winref->usr_pix_buf = (uint32_t *)write_at;
        usr_pix = write_at;

        write_at = pass_to->utask->usr_virt_mmap;
        pass_to->utask->usr_virt_mmap += VMM::SIZE_OF_PAGE;

        uintptr_t virt_ptr = reinterpret_cast<uintptr_t>(winref);
        uint64_t phys_addr = VMM::GetPhysicalAddress(read_cr3(), virt_ptr);
        VMM::map_page(
            (uint64_t *)(pass_to->cr3 + PMM::hhdm_offset), 
            (uint64_t)write_at,
            phys_addr,
            VMM::PTE_PRESENT | 
            VMM::PTE_WRITABLE | 
            VMM::PTE_NX | 
            VMM::PTE_WRITEBACK | 
            VMM::PTE_USER
        );

        return (uint32_t *)write_at;
    }

    Window::~Window() {
        if (!buffer) {
            Debug::krnl_print("R0UI", Debug::LOG_WARN, "Bad window destruction?");
            return;
        }

        lib::Spinlock x(linklock);
        (void)x;

        uint32_t total_bytes = factposn.width * factposn.height * sizeof(uint32_t);
        uint32_t total_pages = (total_bytes + VMM::SIZE_OF_PAGE - 1) / VMM::SIZE_OF_PAGE;

        PMEM::free_pages(buffer, total_pages);

        volatile winpair *tmp_io = linked_io;

        do {
            if (tmp_io->ref == this)
                break;

            tmp_io = tmp_io->next;
        } while (tmp_io != linked_io);

        if (tmp_io->ref != this) {
            Debug::krnl_print("R0UI", Debug::LOG_INFO, "Unable to find self in list.");
            return;
        }

        if (tmp_io == linked_io) {
            linked_io = nullptr;
            Debug::krnl_print("R0UI", Debug::LOG_INFO, "Linked IO is now nullptr!");
        }

        tmp_io->prev->next = tmp_io->next;
        tmp_io->next->prev = tmp_io->prev;
        delete tmp_io;
    }

    void Window::readref(Scheduler::Task *ref) {
        int32_t proposed_x{winref->x}, proposed_y{winref->y};
        uint32_t proposed_w{winref->width}, proposed_h{winref->height};

        Debug::krnl_print(
            "R0UI", 
            Debug::LOG_INFO, 
            "Testing bounds to: {X%i Y%i} x {W%i H%i}",
            proposed_x,
            proposed_y,
            proposed_w,
            proposed_h
        );

        bool size_changed{false};
        size_t og_buf_w = factposn.width;
        size_t og_buf_h = factposn.height;
        size_t og_buf_sz = factposn.height * factposn.width * sizeof(uint32_t);
        size_t og_pages = (og_buf_sz + VMM::SIZE_OF_PAGE - 1) / VMM::SIZE_OF_PAGE;

        if ((uint32_t)proposed_x <= Composer::width)
            factposn.x = proposed_x;
        if ((uint32_t)proposed_y <= Composer::height)
            factposn.y = proposed_y;

        if ((uint32_t)proposed_w <= Composer::width && proposed_w != (uint32_t)factposn.width) {
            size_changed = true;
            factposn.width = proposed_w;
        }
        if ((uint32_t)proposed_h <= Composer::height && proposed_h != (uint32_t)factposn.height) {
            size_changed = true;
            factposn.height = proposed_h;
        }

        winref->x = factposn.x;
        winref->y = factposn.y;
        winref->width = factposn.width;
        winref->height = factposn.height;

        Debug::krnl_print(
            "R0UI", 
            Debug::LOG_INFO, 
            "Updating bounds to: {X%i Y%i} x {W%i H%i}",
            factposn.x,
            factposn.y,
            factposn.width,
            factposn.height
        );

        if (size_changed) {
            uint32_t total_bytes = factposn.width * factposn.height * sizeof(uint32_t);
            uint32_t total_pages = (total_bytes + VMM::SIZE_OF_PAGE - 1) / VMM::SIZE_OF_PAGE;

            uint64_t og_usr_pix = usr_pix;
            usr_pix = ref->utask->usr_virt_mmap;
            winref->usr_pix_buf = (uint32_t *)usr_pix;
            ref->utask->usr_virt_mmap += total_pages * VMM::SIZE_OF_PAGE;

            uint32_t *og_buf = buffer;
            buffer = (uint32_t *)PMEM::alloc_pages(
                total_pages,
                VMM::PTE_PRESENT | 
                VMM::PTE_WRITABLE | 
                VMM::PTE_WRITEBACK | 
                VMM::PTE_NX
            );

            uint64_t cast_buf = (uint64_t)buffer;
            for (auto i{0uz}; i < total_pages; ++i) {
                uint64_t phys_addr = VMM::GetPhysicalAddress(
                    ref->cr3,
                    (uint64_t)(cast_buf + (i * VMM::SIZE_OF_PAGE))
                );
                
                VMM::map_page(
                    (uint64_t *)(ref->cr3 + PMM::hhdm_offset),
                    usr_pix + (i * VMM::SIZE_OF_PAGE),
                    phys_addr,
                    VMM::PTE_PRESENT |
                    VMM::PTE_WRITABLE |
                    VMM::PTE_WRITEBACK |
                    VMM::PTE_NX |
                    VMM::PTE_USER
                );
            }

            uint32_t smaller_w = (proposed_w < og_buf_w) ? proposed_w : og_buf_w;
            uint32_t smaller_h = (proposed_h < og_buf_h) ? proposed_h : og_buf_h;

            Debug::krnl_print("R0UI", Debug::LOG_INFO, "Performing fCopy");

            for (auto i{0uz}; i < smaller_h; ++i) {
                FMEM::FastCopy(
                    &buffer[i * factposn.width], 
                    &og_buf[i * og_buf_w], 
                    smaller_w * sizeof(uint32_t)
                );
            }

            PMEM::free_pages(og_buf, og_pages);
            
            for (auto i{0uz}; i < og_pages; ++i) {
                VMM::unmap_page(
                    (uint64_t *)(ref->cr3 + PMM::hhdm_offset),
                    og_usr_pix + (i * VMM::SIZE_OF_PAGE)
                );
            }
        }

        return;
    }

    void Window::paint(uint32_t *screen) {
        const uint32_t src_stride = factposn.width; 

        for (auto i{0}; i < factposn.height; ++i) {
            uint32_t dest_offset = factposn.x + ((factposn.y + i) * Composer::width);
            uint32_t src_offset  = i * src_stride;

            memcpy(
                &screen[dest_offset],
                &buffer[src_offset],
                factposn.width * sizeof(uint32_t) // Convert pixel count to bytes
            );
        }

        TTY::add_damage(factposn.x, factposn.y, factposn.width, factposn.height);
        HAL::SCREEN::repaint();
    }
}