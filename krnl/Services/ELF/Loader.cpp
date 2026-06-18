#include "Library/string.h"
#include <Services/ELF/ELF.hpp>
#include <Services/VFS/VFS.hpp>

#include <HAL/MEM/PMEM.hpp>
#include <HAL/MEM/PMM.hpp>
#include <HAL/MEM/VMM.hpp>
#include <HAL/MEM/KMEM.hpp>
#include <HAL/DISK/Disk.hpp>

#include <Library/cystr.hpp>
#include <Library/path.hpp>
#include <Library/debug.hpp>
#include <Library/krnlptr.hpp>
#include <Library/regs.h>

using namespace HAL;
using namespace MEM;

namespace ELF {
    /*
    Used for an bad read error during ELF mapping.
    */
    void unload_partial_elf(uint64_t* pml4, ProgramHeader* prog_hdr, size_t failed_index) {
        for (size_t i = 0; i <= failed_index; ++i) {
            if (prog_hdr[i].type != 1) continue;

            auto page_start = prog_hdr[i].vaddr & ~ELF_VADDR_MASK;
            auto page_end   = (prog_hdr[i].vaddr + prog_hdr[i].mem_size + ELF_VADDR_MASK) & ~ELF_VADDR_MASK;

            for (auto vaddr = page_start; vaddr < page_end; vaddr += PAGE_SIZE) {
                uint64_t phys = VMM::GetPhysicalAddress(read_cr3() & VMM::PTE_ADDR_MASK, vaddr);
                if (phys) {
                    VMM::unmap_page(pml4, vaddr);
                    PMM::free_page((void *)phys);
                }
            }
        }
    }
    
    /*
    Loads an app from the passed path (expects a full, lettered path!)
    Note: This loads the app to the current address space. So unless you
    want ring 0 user apps, swap to a ring 3 address space, then load
    the app.
    */
    void *load_elf(lib::string path) {
        Debug::krnl_print("ELF", Debug::LOG_INFO, "Loading %s", path.c_str());
        
        auto fp = lib::parse_path(path);
        auto *root_node = HAL::DISK::GetRootOfDrive(fp.drv);
        
        if (!root_node) {
            Debug::krnl_print("ELF", Debug::LOG_WARN, "Bad path passed! (Invalid drive letter)");
            return nullptr;
        }
    
        lib::sptr<VFS::VNode> target_node = root_node->resolve_path_to_vnode(path);
        if (!target_node) {
            Debug::krnl_print("ELF", Debug::LOG_WARN, "Invalid path to file! (Non-existent!)");
            return nullptr;
        }
    
        Header elf_header;
        if (target_node->read(0, &elf_header, sizeof(Header)) != sizeof(Header)) {
            Debug::krnl_print("ELF", Debug::LOG_WARN, "Failed to read ELF header");
            return nullptr;
        }
    
        if (elf_header.magic != ELF_MAGIC) {
            Debug::krnl_print("ELF", Debug::LOG_WARN, "Invalid ELF header!");
            return nullptr;
        }
    
        auto curr_cr3 = read_cr3() & VMM::PTE_ADDR_MASK;
        auto *app_pml4 = reinterpret_cast<uint64_t *>(curr_cr3 + PMM::hhdm_offset);
    
        auto *prog_hdr = new ProgramHeader[elf_header.ph_count];
        target_node->read(elf_header.ph_offset, prog_hdr, sizeof(ProgramHeader) * elf_header.ph_count);
    
        auto cleanup_and_fail = [&](size_t failed_index) {
            unload_partial_elf(app_pml4, prog_hdr, failed_index);
            delete[] prog_hdr;
            return nullptr;
        };
    
        for (auto i{0uz}; i < elf_header.ph_count; ++i) {
            if (prog_hdr[i].type != 1)
                continue;
        
            auto vaddr_start = prog_hdr[i].vaddr;
            auto vaddr_end   = vaddr_start + prog_hdr[i].mem_size;
            auto page_start  = vaddr_start & ~ELF_VADDR_MASK;
            auto page_end    = (vaddr_end + ELF_VADDR_MASK) & ~ELF_VADDR_MASK;
            auto pages_needed = (page_end - page_start) / PAGE_SIZE;
        
            auto flags = VMM::PTE_PRESENT | VMM::PTE_USER;
            if (prog_hdr[i].flags & ELF_PF_W) flags |= VMM::PTE_WRITABLE;
            
            if (!(prog_hdr[i].flags & ELF_PF_X)) {
                flags |= VMM::PTE_NX; 
            }
        
            auto segment_file_offset{0uz};
        
            for (auto y{0uz}; y < pages_needed; ++y) {
                auto curr_vaddr = page_start + (y * PAGE_SIZE);
            
                auto existing_phys = VMM::GetPhysicalAddress(curr_cr3, curr_vaddr);
                void *phys_page{nullptr};
            
                if (existing_phys) {
                    phys_page = (void *)existing_phys;
                } else {
                    phys_page = PMM::alloc_page();
                    if (!phys_page) return cleanup_and_fail(i);
                    
                    VMM::map_page(app_pml4, curr_vaddr, (uint64_t)phys_page, flags);
                    memset((uint8_t *)((uint64_t)phys_page + PMM::hhdm_offset), 0, PAGE_SIZE);
                }
            
                auto page_hhdm = (uint8_t *)((uint64_t)phys_page + PMM::hhdm_offset);
            
                auto page_offset{0uz};
                if (curr_vaddr < vaddr_start) {
                    page_offset = vaddr_start - curr_vaddr;
                }
            
                auto file_remaining{0uz};
                if (segment_file_offset < prog_hdr[i].file_size) {
                    file_remaining = prog_hdr[i].file_size - segment_file_offset;
                }
            
                auto cp_len = PAGE_SIZE - page_offset;
                if (cp_len > file_remaining)
                    cp_len = file_remaining;
            
                if (cp_len) {
                    size_t bytes_read = target_node->read(prog_hdr[i].offset + segment_file_offset, page_hhdm + page_offset, cp_len);
                
                    if (bytes_read != cp_len) {
                        Debug::krnl_print("ELF", Debug::LOG_ERROR, "VFS read mismatch or error during loading!");
                        return cleanup_and_fail(i);
                    }
                
                    segment_file_offset += cp_len;
                }
            }
        }
    
        delete[] prog_hdr;
    
        Debug::krnl_print("ELF", Debug::LOG_INFO, "Load complete, entry: %llx", elf_header.entry_point);
        return (void *)elf_header.entry_point;
    }
}