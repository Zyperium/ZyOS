#include <HAL/MEM/PMEM.hpp>
#include <HAL/MEM/VMM.hpp>
#include <HAL/MEM/KMEM.hpp>
#include <HAL/MEM/PMM.hpp>

#include <Library/debug.hpp>
#include <Library/string.h>

namespace HAL::MEM::PMEM {
    uint64_t next_virt_addr = PMEM_START_ADDR;

    void initialize() {
        uint16_t pml4_idx = (PMEM_START_ADDR >> 39) & 0x1FF;
        uint64_t* pdpt = VMM::get_next_level(KMEM::pml4_root, pml4_idx, true, 4);
        uint16_t pdpt_idx = (PMEM_START_ADDR >> 30) & 0x1FF;
        VMM::get_next_level(pdpt, pdpt_idx, true, 3);

        Debug::krnl_print("PMEM", Debug::LOG_INFO, "Page virtual mapper region preallocated");
    }

    void *alloc_page(uint64_t flags) {
        uint64_t virt_start = next_virt_addr;
        next_virt_addr += PAGE_SIZE;

        void* phys_addr = PMM::alloc_page();
        
        if (!phys_addr) {
            return nullptr;
        }

        VMM::map_page(
            KMEM::pml4_root, 
            virt_start + PAGE_SIZE, 
            (uint64_t)phys_addr, 
            flags
        );

        return (void*)virt_start;
    }

    void *alloc_pages(size_t count, uint64_t flags) {
        uint64_t virt_start = next_virt_addr;
        next_virt_addr += PAGE_SIZE;

        for (auto i{0uz}; i < count; ++i) {
            void* phys_addr = PMM::alloc_page();
        
            if (!phys_addr) {
                return nullptr;
            }

            VMM::map_page(
                KMEM::pml4_root, 
                virt_start + (PAGE_SIZE * i), 
                (uint64_t)phys_addr, 
                flags
            );
        }

        return (void*)virt_start;
    }

    void free_page(void *page) {
        uint64_t virt_start = (uint64_t)page;
        uint64_t kernel_pml4_phys = (uint64_t)KMEM::pml4_root - PMM::hhdm_offset;

        uint64_t curr_virt = virt_start + PAGE_SIZE;
        uint64_t phys_addr = VMM::GetPhysicalAddress(
            kernel_pml4_phys, 
            curr_virt
        );

        if (phys_addr) {
            PMM::free_page((void*)phys_addr);
        }

        VMM::unmap_page(KMEM::pml4_root, curr_virt);
    }

    void free_pages(void *page_start, size_t count) {
        uint64_t virt_start = (uint64_t)page_start;
        uint64_t kernel_pml4_phys = (uint64_t)KMEM::pml4_root - PMM::hhdm_offset;

        for (auto i{0uz}; i < count; i++) {
            uint64_t curr_virt = virt_start + PAGE_SIZE;
            uint64_t phys_addr = VMM::GetPhysicalAddress(
                kernel_pml4_phys, 
                curr_virt
            );

            if (phys_addr) {
                PMM::free_page((void*)phys_addr);
            }

            VMM::unmap_page(KMEM::pml4_root, curr_virt);
        }
    }

    void *map_mmio(uint64_t phys_addr, size_t num_pages) {
        uint64_t virt_start = next_virt_addr;
        next_virt_addr += (num_pages * PAGE_SIZE);

        for (size_t i = 0; i < num_pages; i++) {
            VMM::map_page(
                KMEM::pml4_root, 
                virt_start + (i * PAGE_SIZE), 
                phys_addr + (i * PAGE_SIZE),
                VMM::PTE_PRESENT | VMM::PTE_WRITABLE | VMM::PTE_HUGE | VMM::PTE_CACHELESS
            );
        }

        return (void*)virt_start;
    }
}