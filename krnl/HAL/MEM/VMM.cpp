#include <HAL/MEM/VMM.hpp>
#include <HAL/MEM/PMM.hpp>

#include <Library/string.h>

namespace HAL::MEM::VMM {
    uint64_t *get_next_level(uint64_t *current_table, uint64_t index, bool allocate, int level, uint64_t target_flags) {
        uint64_t entry = current_table[index];

        if (entry & PTE_PRESENT) {
            uint64_t new_flags = entry;
            bool changed = false;

            if ((target_flags & PTE_USER) && !(entry & PTE_USER)) {
                new_flags |= PTE_USER;
                changed = true;
            }
            if ((target_flags & PTE_WRITABLE) && !(entry & PTE_WRITABLE)) {
                new_flags |= PTE_WRITABLE;
                changed = true;
            }

            if (changed) {
                current_table[index] = new_flags;
                uint64_t cr3_val;
                asm volatile("mov %%cr3, %0" : "=r"(cr3_val));
                asm volatile("mov %0, %%cr3" :: "r"(cr3_val) : "memory");
            }

            if (entry & PTE_HUGE) {
                if (!allocate) return nullptr;

                void* new_table_phys = PMM::alloc_page();
                if (!new_table_phys) return nullptr;

                uint64_t *new_table_virt = (uint64_t *)((uint64_t)new_table_phys + PMM::hhdm_offset);
                memset(new_table_virt, 0, 4096);

                uint64_t huge_phys_base = entry & PTE_ADDR_MASK;
                uint64_t flags = entry & PTE_ADDR_MASK;

                if (changed) {
                    flags |= new_flags;
                }

                uint64_t size_increment = 0;

                if (level == 3) {
                    size_increment = 0x200000; // 2MB
                    flags |= PTE_HUGE;
                }
                else if (level == 2) {
                    size_increment = 0x1000; // 4KB
                    flags &= ~PTE_HUGE;
                }

                for (uint64_t i = 0; i < 512; i++) {
                    uint64_t offset = i * size_increment;
                    new_table_virt[i] = (huge_phys_base + offset) | flags | (target_flags & (PTE_USER | PTE_WRITABLE));
                }

                uint64_t new_entry = (uint64_t)new_table_phys | PTE_PRESENT | PTE_WRITABLE | PTE_USER;
                current_table[index] = new_entry;

                uint64_t cr3_val;
                asm volatile("mov %%cr3, %0" : "=r"(cr3_val));
                asm volatile("mov %0, %%cr3" :: "r"(cr3_val) : "memory");

                return new_table_virt;
            }

            uint64_t phys_addr = entry & PTE_ADDR_MASK;
            return (uint64_t*)(phys_addr + PMM::hhdm_offset);
        }

        if (!allocate) return nullptr;

        void* new_table_phys = PMM::alloc_page();
        if (!new_table_phys) return nullptr;

        uint64_t* new_table_virt = (uint64_t*)((uint64_t)new_table_phys + PMM::hhdm_offset);
        memset(new_table_virt, 0, 4096);

        uint64_t dir_flags = PTE_PRESENT | PTE_WRITABLE;
        if (target_flags & PTE_USER) dir_flags |= PTE_USER;

        current_table[index] = (uint64_t)new_table_phys | dir_flags;

        return new_table_virt;
    }

    void map_page(uint64_t *pml4_root, uint64_t virt, uint64_t phys, uint64_t flags) {
        uint16_t pml4_idx = (virt >> 39) & 0x1FF;
        uint16_t pdpt_idx = (virt >> 30) & 0x1FF;
        uint16_t pd_idx   = (virt >> 21) & 0x1FF;
        uint16_t pt_idx   = (virt >> 12) & 0x1FF;

        uint64_t *pdpt = get_next_level(pml4_root, pml4_idx, true, 4, flags);
        if (!pdpt) {
            return;
        }

        uint64_t *pd = get_next_level(pdpt, pdpt_idx, true, 3, flags);
        if (!pd) {
            return;
        }

        uint64_t *pt = get_next_level(pd, pd_idx, true, 2, flags);
        if (!pt) {
            return;
        }

        pt[pt_idx] = phys | flags;
        
        asm volatile("invlpg (%0)" :: "r" (virt) : "memory");
    }

    void unmap_page(uint64_t *pml4_root, uint64_t virt) {
        uint16_t pml4_idx = (virt >> 39) & 0x1FF;
        uint16_t pdpt_idx = (virt >> 30) & 0x1FF;
        uint16_t pd_idx   = (virt >> 21) & 0x1FF;
        uint16_t pt_idx   = (virt >> 12) & 0x1FF;

        uint64_t* pdpt = get_next_level(pml4_root, pml4_idx, false, 4);
        if (!pdpt) return;
        uint64_t* pd = get_next_level(pdpt, pdpt_idx, false, 3);
        if (!pd) return;
        uint64_t* pt = get_next_level(pd, pd_idx, false, 2);
        if (!pt) return;
        uint64_t phys = pt[pt_idx] & PTE_ADDR_MASK;

        if (phys != 0) {
            pt[pt_idx] = 0;
            PMM::free_page((void*)phys);
        }

        uint64_t cr3_val;
        asm volatile("mov %%cr3, %0" : "=r"(cr3_val));
        asm volatile("invlpg (%0)" :: "r" (virt) : "memory");
    }

    uint64_t CreateProcessPageTable(uint64_t kernel_pml4_phys) {
        uint64_t* new_pml4_phys = (uint64_t*)PMM::alloc_page();

        uint64_t* new_pml4_virt = (uint64_t*)((uint64_t)new_pml4_phys + PMM::hhdm_offset);

        memset(new_pml4_virt, 0, 4096);

        uint64_t* kernel_pml4_virt = (uint64_t*)(kernel_pml4_phys + PMM::hhdm_offset);

        for (int i = 256; i < 512; i++) {
            new_pml4_virt[i] = kernel_pml4_virt[i];
        }

        return (uint64_t)new_pml4_phys;
    }

    uint64_t GetPhysicalAddress(uint64_t cr3, uint64_t virtAddr) {
        uint64_t pml4_idx = (virtAddr >> 39) & 0x1FF;
        uint64_t pdpt_idx = (virtAddr >> 30) & 0x1FF;
        uint64_t pd_idx   = (virtAddr >> 21) & 0x1FF;
        uint64_t pt_idx   = (virtAddr >> 12) & 0x1FF;

        uint64_t* pml4 = (uint64_t*)((cr3 & PTE_ADDR_MASK) + PMM::hhdm_offset);
        if (!(pml4[pml4_idx] & 1)) return 0;

        uint64_t* pdpt = (uint64_t*)((pml4[pml4_idx] & PTE_ADDR_MASK) + PMM::hhdm_offset);
        if (!(pdpt[pdpt_idx] & 1)) return 0;

        if (pdpt[pdpt_idx] & 0x80) {
            uint64_t pagePhys = pdpt[pdpt_idx] & PTE_ADDR_MASK & ~0x3FFFFFFF;
            return pagePhys + (virtAddr & 0x3FFFFFFF);
        }

        uint64_t* pd = (uint64_t*)((pdpt[pdpt_idx] & PTE_ADDR_MASK) + PMM::hhdm_offset);
        if (!(pd[pd_idx] & 1)) return 0;

        if (pd[pd_idx] & 0x80) {
            uint64_t pagePhys = pd[pd_idx] & PTE_ADDR_MASK & ~0x1FFFFF;
            return pagePhys + (virtAddr & 0x1FFFFF);
        }

        uint64_t* pt = (uint64_t*)((pd[pd_idx] & PTE_ADDR_MASK) + PMM::hhdm_offset);
        if (!(pt[pt_idx] & 1)) return 0;

        uint64_t physBase = pt[pt_idx] & PTE_ADDR_MASK;
        return physBase + (virtAddr & 0xFFF);
    }

    void FreeProcessPages(uint64_t cr3) {
        if (cr3 == 0) return;

        uint64_t *pml4 = (uint64_t*)((cr3 & PTE_ADDR_MASK) + PMM::hhdm_offset);

        for (int i = 0; i < 256; i++) {
            if (pml4[i] & 1) {
                uint64_t *pdpt = (uint64_t*)((pml4[i] & PTE_ADDR_MASK) + PMM::hhdm_offset);

                for (int j = 0; j < 512; j++) {
                    if (pdpt[j] & 1) {
                        if (pdpt[j] & PTE_HUGE) {
                            PMM::free_page((void*)(pdpt[j] & PTE_ADDR_MASK));
                            continue;
                        }

                        uint64_t *pd = (uint64_t*)((pdpt[j] & PTE_ADDR_MASK) + PMM::hhdm_offset);

                        for (int k = 0; k < 512; k++) {
                            if (pd[k] & 1) {
                                if (pd[k] & PTE_HUGE) {
                                    PMM::free_page((void*)(pd[k] & PTE_ADDR_MASK));
                                    continue;
                                }

                                uint64_t* pt = (uint64_t*)((pd[k] & PTE_ADDR_MASK) + PMM::hhdm_offset);

                                for (int l = 0; l < 512; l++) {
                                    if (pt[l] & 1) {
                                        uint64_t phys_addr = pt[l] & PTE_ADDR_MASK;
                                        PMM::free_page((void*)phys_addr);
                                    }
                                }
                                PMM::free_page((void*)(pd[k] & PTE_ADDR_MASK));
                            }
                        }
                        PMM::free_page((void*)(pdpt[j] & PTE_ADDR_MASK));
                    }
                }
                PMM::free_page((void*)(pml4[i] & PTE_ADDR_MASK));
            }
        }
        PMM::free_page((void*)(cr3 & PTE_ADDR_MASK));
    }
}