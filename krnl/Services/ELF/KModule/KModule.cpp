#include <Library/string.h>
#include <Library/debug.hpp>
#include <Library/krnlptr.hpp>
#include <Library/path.hpp>

#include <Services/ELF/KModule/KModule.hpp>
#include <Services/ELF/ELF.hpp>
#include <Services/Scheduler/Scheduler.hpp>

#include <HAL/DISK/Disk.hpp>
#include <HAL/MEM/PMEM.hpp>
#include <HAL/MEM/KMEM.hpp>
#include <HAL/MEM/VMM.hpp>

#include <Services/VFS/VFS.hpp>
#include <Services/VFS/FAT32/FAT32.hpp>

using namespace HAL;
using namespace MEM;

namespace ELF::KModule {
    HashTableHeader *kernel_symbols{};

    constexpr const char *FIXED_SYMBOL_PATH = "A:/SYSTEM/KHASH.MAP";
    void initialize() {
        Debug::krnl_print("KMOD", Debug::LOG_INFO, "Initialize");
        lib::fullpath parsed_path = lib::parse_path(FIXED_SYMBOL_PATH);

        while (!DISK::root_disk_id) {
            Scheduler::Yield();
            asm volatile("pause");
        }

        auto *root_disk = DISK::GetDisk(parsed_path.drv);
        lib::sptr<VFS::VNode> kmap_node = root_disk->rootnode->resolve_path_to_vnode(parsed_path.path);
        kernel_symbols = new HashTableHeader;
        kmap_node->read(0, kernel_symbols, sizeof(uint64_t));
        Debug::krnl_print("KMOD", Debug::LOG_INFO, "Reading %i symbols", kernel_symbols->symbol_count);

        kernel_symbols->elements = new HashedElement[kernel_symbols->symbol_count];
        kmap_node->read(sizeof(uint64_t), kernel_symbols->elements, kernel_symbols->symbol_count * sizeof(HashedElement));

        Scheduler::Suicide();
        for (;;);
    }

    /*
    This expects the path to be a DRIVE letter starting path.
    E.G. A:/SYSTEM/DRIVERS/LinuxCMPT.kmo
    E.G. C:/<USER>/my_driver.kmo
    */
    void *load_module(lib::string path) {
        lib::fullpath parsed_path = lib::parse_path(path);

        if (!parsed_path.drv) {
            Debug::krnl_print("KMOD", Debug::LOG_WARN, "Received bad path to kernel module.");
            return nullptr;
        }

        auto *root_disk = DISK::GetDisk(parsed_path.drv);
        if (!root_disk) {
            Debug::krnl_print("KMOD", Debug::LOG_WARN, "Received bad path to kernel module.");
            return nullptr;
        }

        lib::sptr<VFS::VNode> module_node = root_disk->rootnode->resolve_path_to_vnode(parsed_path.path);

        if (!module_node || module_node->get_size() == 0) {
            Debug::krnl_print("KMOD", Debug::LOG_WARN, "Bad module path or empty file.");
            return nullptr;
        }

        Header hdr;
        module_node->read(0, &hdr, sizeof(Header));

        if (hdr.magic != ELF_MAGIC || hdr.type != ELF_RELOCATABLE) {
            Debug::krnl_print("KMOD", Debug::LOG_WARN, "Invalid or non-relocatable ELF.");
            return nullptr;
        }

        auto *sections = new SectionHeader[hdr.sh_count];
        module_node->read(hdr.sh_offset, sections, hdr.sh_count * hdr.sh_entry_size);

        auto *shstrtab_hdr = &sections[hdr.sh_str_index];
        auto *shstrtab = new int8_t[shstrtab_hdr->size];
        module_node->read(shstrtab_hdr->offset, shstrtab, shstrtab_hdr->size);

        auto *section_addrs = (uint64_t *)KMEM::calloc(hdr.sh_count, sizeof(uint64_t));
        auto entry_address{0uz};

        for (auto i{0uz}; i < hdr.sh_count; ++i) {
            if (!(sections[i].flags & SHF_ALLOC)) continue;

            auto align = (sections[i].addralign < MIN_ALIGN) ? MIN_ALIGN : sections[i].addralign;
            void *sec_mem = KMEM::malloc(sections[i].size + align);

            auto aligned_addr = ((uint64_t)sec_mem + (align - 1)) & ~(align - 1);
            section_addrs[i] = aligned_addr;

            if (sections[i].type == SHT_NOBITS) {
                memset((void *)aligned_addr, 0, sections[i].size);
            } else {
                module_node->read(sections[i].offset, (void *)aligned_addr, sections[i].size);
            }
        }

        Symbol* global_symtab = nullptr;
        char* global_strtab = nullptr;
        size_t global_sym_count = 0;

        for (auto i{0uz}; i < hdr.sh_count; ++i) {
            if (sections[i].type == SHT_SYMTAB) {
                auto *symtab_hdr = &sections[i];
                auto *strtab_hdr = &sections[symtab_hdr->link];

                global_sym_count = symtab_hdr->size / sizeof(Symbol);
                global_symtab = new Symbol[global_sym_count];
                module_node->read(symtab_hdr->offset, global_symtab, symtab_hdr->size);

                global_strtab = new char[strtab_hdr->size];
                module_node->read(strtab_hdr->offset, global_strtab, strtab_hdr->size);
                break;
            }
        }

        if (global_symtab && global_strtab) {
            for (auto i{0uz}; i < hdr.sh_count; ++i) {
                if (sections[i].type != SHT_RELA) continue;

                auto target_base = section_addrs[sections[i].info];
                auto rela_count = sections[i].size / sizeof(Rela);
                auto *relocs = new Rela[rela_count];
                module_node->read(sections[i].offset, relocs, sections[i].size);

                for (auto r{0uz}; r < rela_count; ++r) {
                    auto *rela = &relocs[r];
                    auto sym_idx = rela->info >> RELA_INFO_SHIFT;
                    auto type = rela->info & RELA_INFO_MASK;

                    auto *sym = &global_symtab[sym_idx];
                    auto sym_addr{0uz};

                    if (sym->shndx != 0 && sym->shndx < hdr.sh_count) {
                        sym_addr = section_addrs[sym->shndx] + sym->value;
                    } else if (sym->shndx == 0) {
                        sym_addr = resolve_symbol(global_strtab + sym->name);
                    }

                    uint64_t patch_site = target_base + rela->offset;

                    switch (type) {
                        case R_X86_64_64: {
                            *reinterpret_cast<uint64_t *>(patch_site) = sym_addr + rela->addend;
                            break;
                        }
                        case R_X86_64_PC32:
                        case R_X86_64_PLT32: {
                            auto res = (sym_addr + rela->addend) - patch_site - RES_OFFSET;
                            *reinterpret_cast<uint32_t *>(patch_site) = static_cast<uint32_t>(res);
                            break;
                        }
                    }
                }
                delete[] relocs;
            }

            for (auto s{0uz}; s < global_sym_count; ++s) {
                auto *sym = &global_symtab[s];
                if ((sym->info >> SYM_INFO_SHIFT) == STB_GLOBAL && sym->shndx != 0 && sym->shndx < hdr.sh_count) {
                    const char* sym_name = global_strtab + sym->name;
                    
                    if (strcmp(sym_name, "init_module")) {
                        entry_address = section_addrs[sym->shndx] + sym->value;
                        continue;
                    }

                    export_symbol(sym_name, section_addrs[sym->shndx] + sym->value);
                }
            }
        }

        delete[] global_symtab;
        delete[] global_strtab;
        delete[] sections;
        delete[] shstrtab;
        delete[] section_addrs;

        if (!entry_address) {
            Debug::krnl_print("KMOD", Debug::LOG_INFO, "Library module loaded!");
            return nullptr;
        }

        Debug::krnl_print("KMOD", Debug::LOG_INFO, "Module loaded successfully!");
        return reinterpret_cast<void *>(entry_address);
    }

    uint64_t resolve_symbol(const char *symbol) {
        if (!kernel_symbols) {
            Debug::krnl_print("KMOD", Debug::LOG_WARN, "Kernel symbols aren't ready?");
            return -1;
        }

        uint32_t target_hash = _hash(symbol);
        int64_t low = 0;
        int64_t high = static_cast<int64_t>(kernel_symbols->symbol_count) - 1;

        while (low <= high) {
            int64_t mid = low + (high - low) / 2;
            uint32_t mid_hash = kernel_symbols->elements[mid].hash;

            if (mid_hash == target_hash) {
                Debug::krnl_print("KMOD", Debug::LOG_INFO, "Resolved symbol %s", symbol);
                return kernel_symbols->elements[mid].address;
            }
            if (mid_hash < target_hash) {
                low = mid + 1;
            } else {
                high = mid - 1;
            }
        }

        Debug::krnl_print("KMOD", Debug::LOG_WARN, "Unable to resolve symbol %s", symbol);
        return 0;
    }

    void export_symbol(const char *symbol, uint64_t kernel_address) {
        (void)symbol;
        (void)kernel_address;
        return;
    }
}