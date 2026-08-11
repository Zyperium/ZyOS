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

        uint32_t total_bytes = factposn.width * factposn.height * sizeof(uint32_t);
        uint32_t total_pages = (total_bytes + VMM::SIZE_OF_PAGE - 1) / VMM::SIZE_OF_PAGE;

        PMEM::free_pages(buffer, total_pages);
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