#include <Library/debug.hpp>
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

    void set_write_combining(uint64_t *pml4_root, uint64_t virt_start, size_t page_count) {
        uint64_t virt_end = virt_start + (page_count * 0x1000);
        uint64_t virt = virt_start;

        while (virt < virt_end) {
            uint64_t pml4_idx = (virt >> 39) & 0x1FF;
            uint64_t pdpt_idx = (virt >> 30) & 0x1FF;
            uint64_t pd_idx   = (virt >> 21) & 0x1FF;
            uint64_t pt_idx   = (virt >> 12) & 0x1FF;

            uint64_t *pdpt = get_next_level(pml4_root, pml4_idx, false, 4);
            if (!pdpt || !(*pdpt & PTE_PRESENT)) { virt += 0x1000; continue; }

            uint64_t *pd = get_next_level(pdpt, pdpt_idx, false, 3);
            if (!pd || !(*pd & PTE_PRESENT)) { virt += 0x1000; continue; }

            uint64_t *pde = &pd[pd_idx];
            if (!(*pde & PTE_PRESENT)) { virt += 0x1000; continue; }

            if (*pde & PTE_HUGE) {
                *pde &= ~PTE_CACHELESS;
                *pde |=  PTE_WRITEBACK;
                *pde &= ~(1ULL << 12);

                Debug::krnl_print("VMM", Debug::LOG_INFO, "PDE is now set to writeback (%x)", *pde);
                asm volatile("invlpg (%0)" :: "r" (virt) : "memory");
                virt += 0x200000;
            } else {
                uint64_t *pt = get_next_level(pd, pd_idx, false, 2);
                if (!pt || !(*pt & PTE_PRESENT)) { virt += 0x1000; continue; }

                uint64_t *pte = &pt[pt_idx];
                if (!(*pte & PTE_PRESENT)) { virt += 0x1000; continue; }

                *pte &= ~PTE_CACHELESS;
                *pte |=  PTE_WRITEBACK;
                *pte &= ~(1ULL << 7);

                asm volatile("invlpg (%0)" :: "r" (virt) : "memory");
                Debug::krnl_print("VMM", Debug::LOG_INFO, "PTE is now set to writeback (%x)", *pte);
                virt += 0x1000;
            }
        }
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
        PMM::reference_page((void *)phys);
        
        asm volatile("invlpg (%0)" :: "r" (virt) : "memory");
        asm volatile("sfence" ::: "memory");
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

        asm volatile("mfence" ::: "memory");
        asm volatile("sfence" ::: "memory");
        asm volatile("lfence" ::: "memory");

        return (uint64_t)new_pml4_phys;
    }


    uint64_t ClonePageDirectory(uint64_t cr3_to_clone) {
        uint64_t new_pml4_phys = CreateProcessPageTable(cr3_to_clone);

        uint64_t* src_pml4 = (uint64_t*)(cr3_to_clone + PMM::hhdm_offset);
        uint64_t* dst_pml4 = (uint64_t*)(new_pml4_phys + PMM::hhdm_offset);

        constexpr uint64_t PHYS_ADDR_MASK = 0x000FFFFFFFFFF000ULL;

        for (int pml4_i = 0; pml4_i < 256; pml4_i++) {
            if (!(src_pml4[pml4_i] & 1)) continue;

            uint64_t src_pdpt_phys = src_pml4[pml4_i] & PHYS_ADDR_MASK;
            uint64_t* src_pdpt = (uint64_t*)(src_pdpt_phys + PMM::hhdm_offset);

            uint64_t dst_pdpt_phys = (uint64_t)PMM::alloc_page();
            uint64_t* dst_pdpt = (uint64_t*)(dst_pdpt_phys + PMM::hhdm_offset);
            memset(dst_pdpt, 0, PAGE_SIZE);

            dst_pml4[pml4_i] = dst_pdpt_phys | (src_pml4[pml4_i] & 0xFFFULL);

            for (int pdpt_i = 0; pdpt_i < 512; pdpt_i++) {
                if (!(src_pdpt[pdpt_i] & 1)) continue;
                if (src_pdpt[pdpt_i] & (1 << 7)) continue; 

                uint64_t src_pd_phys = src_pdpt[pdpt_i] & PHYS_ADDR_MASK;
                uint64_t* src_pd = (uint64_t*)(src_pd_phys + PMM::hhdm_offset);

                uint64_t dst_pd_phys = (uint64_t)PMM::alloc_page();
                uint64_t* dst_pd = (uint64_t*)(dst_pd_phys + PMM::hhdm_offset);
                memset(dst_pd, 0, PAGE_SIZE);

                dst_pdpt[pdpt_i] = dst_pd_phys | (src_pdpt[pdpt_i] & 0xFFFULL);

                for (int pd_i = 0; pd_i < 512; pd_i++) {
                    if (!(src_pd[pd_i] & 1)) continue;
                    if (src_pd[pd_i] & (1 << 7)) continue;

                    uint64_t src_pt_phys = src_pd[pd_i] & PHYS_ADDR_MASK;
                    uint64_t* src_pt = (uint64_t*)(src_pt_phys + PMM::hhdm_offset);

                    uint64_t dst_pt_phys = (uint64_t)PMM::alloc_page();
                    uint64_t* dst_pt = (uint64_t*)(dst_pt_phys + PMM::hhdm_offset);
                    memset(dst_pt, 0, PAGE_SIZE);

                    dst_pd[pd_i] = dst_pt_phys | (src_pd[pd_i] & 0xFFFULL);

                    for (int pt_i = 0; pt_i < 512; pt_i++) {
                        if (!(src_pt[pt_i] & 1)) continue;

                        uint64_t src_frame_phys = src_pt[pt_i] & PHYS_ADDR_MASK;
                        uint64_t dst_frame_phys = (uint64_t)PMM::alloc_page();

                        memcpy((void*)(dst_frame_phys + PMM::hhdm_offset),
                               (void*)(src_frame_phys + PMM::hhdm_offset),
                               PAGE_SIZE);

                        dst_pt[pt_i] = dst_frame_phys | (src_pt[pt_i] & 0xFFFULL);
                        asm volatile("mfence" ::: "memory");
                        asm volatile("sfence" ::: "memory");
                        asm volatile("lfence" ::: "memory");
                    }
                }
            }
        }

        return new_pml4_phys;
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
                                
                                asm volatile("mfence" ::: "memory");
                                asm volatile("sfence" ::: "memory");
                                asm volatile("lfence" ::: "memory");
                                PMM::free_page((void*)(pd[k] & PTE_ADDR_MASK));
                            }
                        }

                        asm volatile("mfence" ::: "memory");
                        asm volatile("sfence" ::: "memory");
                        asm volatile("lfence" ::: "memory");
                        PMM::free_page((void*)(pdpt[j] & PTE_ADDR_MASK));
                    }
                }

                asm volatile("mfence" ::: "memory");
                asm volatile("sfence" ::: "memory");
                asm volatile("lfence" ::: "memory");
                PMM::free_page((void*)(pml4[i] & PTE_ADDR_MASK));
            }
        }

        asm volatile("mfence" ::: "memory");
        asm volatile("sfence" ::: "memory");
        asm volatile("lfence" ::: "memory");
        PMM::free_page((void*)(cr3 & PTE_ADDR_MASK));
    }
}