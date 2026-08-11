#include <Library/string.h>
#include <Services/ELF/ELF.hpp>
#include <Services/VFS/VFS.hpp>

#include <HAL/MEM/PMEM.hpp>
#include <HAL/MEM/PMM.hpp>
#include <HAL/MEM/VMM.hpp>
#include <HAL/MEM/KMEM.hpp>
#include <HAL/MEM/FMEM.hpp>
#include <HAL/DISK/Disk.hpp>
#include <HAL/ACPI/ACPI.hpp>

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

    constexpr size_t ELF_READ_CHUNK = 1 * 1024 * 1024; // 512kb

    void *load_elf(lib::string path) {
        Debug::krnl_print("ELF", Debug::LOG_INFO, "Loading %s (starting clock)", path.c_str());
        auto start_t = ACPI::get_sys_time();
        auto fp = lib::parse_path(path);
        auto *root_node = HAL::DISK::GetRootOfDrive(fp.drv);

        if (!root_node) {
            Debug::krnl_print("ELF", Debug::LOG_WARN, "Bad path passed! (Invalid drive letter)");
            return nullptr;
        }

        lib::sptr<VFS::VNode> target_node = root_node->resolve_path_to_vnode(fp.path);

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
        auto ph_table_size = sizeof(ProgramHeader) * elf_header.ph_count;
        if (target_node->read(elf_header.ph_offset, prog_hdr, ph_table_size) != (int)ph_table_size) {
            Debug::krnl_print("ELF", Debug::LOG_WARN, "Failed to read program headers");
            delete[] prog_hdr;
            return nullptr;
        }

        auto *stage = new uint8_t[ELF_READ_CHUNK];

        auto cleanup_and_fail = [&](size_t failed_index) {
            unload_partial_elf(app_pml4, prog_hdr, failed_index);
            delete[] stage;
            delete[] prog_hdr;
            return nullptr;
        };

        size_t phase1_t{0}, phase2_t{0};
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

            auto *phys_pages = new uint64_t[pages_needed];

            for (auto y{0uz}; y < pages_needed; ++y) {
                auto curr_vaddr = page_start + (y * PAGE_SIZE);

                auto existing_phys = VMM::GetPhysicalAddress(curr_cr3, curr_vaddr);

                if (existing_phys) {
                    phys_pages[y] = existing_phys;
                    continue;
                }

                void *phys_page = PMM::alloc_page();
                if (!phys_page) {
                    delete[] phys_pages;
                    return cleanup_and_fail(i);
                }

                VMM::map_page(app_pml4, curr_vaddr, (uint64_t)phys_page, flags);
                memset((uint8_t *)((uint64_t)phys_page + PMM::hhdm_offset), 0, PAGE_SIZE);
                phys_pages[y] = (uint64_t)phys_page;
            }

            if (!phase1_t)
                phase1_t = ACPI::get_sys_time() - start_t;

            auto file_size = prog_hdr[i].file_size;
            auto file_off  = 0uz;

            while (file_off < file_size) {
                auto chunk_len = file_size - file_off;
                if (chunk_len > ELF_READ_CHUNK) chunk_len = ELF_READ_CHUNK;

                size_t bytes_read = target_node->read(prog_hdr[i].offset + file_off, stage, chunk_len);
                if (bytes_read != chunk_len) {
                    Debug::krnl_print("ELF", Debug::LOG_ERROR, "VFS read mismatch or error during loading!");
                    delete[] phys_pages;
                    return cleanup_and_fail(i);
                }

                auto remaining  = chunk_len;
                auto stage_off  = 0uz;
                auto dest_vaddr = vaddr_start + file_off;

                while (remaining) {
                    auto page_vaddr  = dest_vaddr & ~ELF_VADDR_MASK;
                    auto in_page_off = dest_vaddr - page_vaddr;
                    auto copy_len    = PAGE_SIZE - in_page_off;
                    if (copy_len > remaining) copy_len = remaining;

                    auto page_idx  = (page_vaddr - page_start) / PAGE_SIZE;
                    auto *page_hhdm = (uint8_t *)(phys_pages[page_idx] + PMM::hhdm_offset);

                    FMEM::FastCopy(page_hhdm + in_page_off, stage + stage_off, copy_len);

                    dest_vaddr += copy_len;
                    stage_off  += copy_len;
                    remaining  -= copy_len;
                }

                file_off += chunk_len;
            }

            if (!phase2_t)
                phase2_t = ACPI::get_sys_time() - start_t - phase1_t;

            delete[] phys_pages;
        }


        delete[] stage;
        delete[] prog_hdr;

        size_t total_time = ACPI::get_sys_time() - start_t;

        Debug::krnl_print("ELF", Debug::LOG_INFO, "Load complete, entry: %x (t: %ims, p1: %ims, p2: %ims)", elf_header.entry_point, total_time, phase1_t, phase2_t);
        if (elf_header.header_size == 0) {
            Debug::krnl_print("ELF", Debug::LOG_WARN, "Something is up with then read file!?");
        }
        return (void *)elf_header.entry_point;
    }
}