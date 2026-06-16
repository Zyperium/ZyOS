#include "HAL/MEM/KMEM.hpp"
#include <Library/string.h>
#include <Library/debug.hpp>
#include <Library/krnlptr.hpp>

#include <Services/ELF//KModule/KModule.hpp>
#include <Services/ELF/ELF.hpp>

#include <HAL/DISK/Disk.hpp>
#include <HAL/MEM/PMEM.hpp>
#include <HAL/MEM/KMEM.hpp>
#include <HAL/MEM/VMM.hpp>

#include <VFS/VFS.hpp>
#include <VFS/FAT32/FAT32.hpp>

using namespace HAL;
using namespace MEM;

namespace ELF::KModule {
    void initialize() {

    }

    /*
    This expects the path to be a DRIVE letter starting path.
    E.G. A:/SYSTEM/DRIVERS/LinuxCMPT.kmo
    E.G. C:/<USER>/my_driver.kmo
    */
    void *load_module(lib::string path) {
        if (path.length() <= MIN_PATH_LENGTH) {
            return nullptr;
        }

        char drv_letter = path[0];
        DISK::Disk *root_disk = DISK::GetDisk(drv_letter);

        if (!root_disk) {
            Debug::krnl_print("KMOD", Debug::LOG_WARN, "Received bad path to kernel module.");
            return nullptr;
        }

        lib::string pure_path;
        for (auto i{MIN_PATH_LENGTH}; i < path.length(); ++i) {
            pure_path += path[i];
        }

        lib::sptr<VFS::VNode> module_node = root_disk->rootnode->resolve_path_to_vnode(pure_path);

        if (!module_node) {
            Debug::krnl_print("KMOD", Debug::LOG_WARN, "Received bad path to kernel module.");
            return nullptr;
        }

        if (module_node->get_size() == 0) {
            Debug::krnl_print("KMOD", Debug::LOG_INFO, "Empty module!?");
            return nullptr;
        }

        Header hdr;
        module_node->read(0, &hdr, sizeof(Header));

        if (hdr.magic != ELF_MAGIC || hdr.type != 1) {
            Debug::krnl_print("LMOD", Debug::LOG_WARN, "Invalid or non-relocatable ELF.");
            return nullptr;
        }

        uint64_t sh_size = hdr.sh_entry_size * hdr.sh_count;
        SectionHeader *sections = new SectionHeader[sh_size];
        module_node->read(hdr.sh_offset, sections, sh_size);

        SectionHeader *shstrtab_hdr = &sections[hdr.sh_str_index];
        char *shstrtab = new char[shstrtab_hdr->size];
        module_node->read(shstrtab_hdr->offset, shstrtab, shstrtab_hdr->size);
        
        uint64_t *section_addrs = (uint64_t *)KMEM::calloc(hdr.sh_count, sizeof(uint64_t));

        (void)section_addrs;
        return nullptr;
    }
}