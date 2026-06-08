#include <HAL/MEM/MEM.hpp>
#include <HAL/MEM/PMM.hpp>
#include <HAL/MEM/VMM.hpp>
#include <HAL/MEM/KMEM.hpp>
#include <HAL/MEM/FMEM.hpp>

#include <Library/regs.h>
#include <Library/debug.hpp>

namespace HAL::MEM {
    void initialize(volatile limine_hhdm_request *hhdm_req, volatile limine_memmap_response *memmap_req) {
        Debug::krnl_print("MEM", Debug::LOG_INFO, "Initialize");
        
        PMM::initialize(memmap_req, hhdm_req);
        uint64_t pml4_phys = read_cr3() & VMM::PTE_ADDR_MASK;
        uint64_t pml4_virt = pml4_phys + PMM::hhdm_offset;

        KMEM::initialize(
            reinterpret_cast<uint64_t*>(pml4_virt), 
            KMEM::DEFAULT_KMEM_START, 
            KMEM::INITIAL_PAGES
        );

        FMEM::enable_sse();
    }
}