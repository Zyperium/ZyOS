#include "Library/path.hpp"
#include <Library/debug.hpp>
#include <Library/string.h>
#include <Library/krnlptr.hpp>

#include <Services/VFS/VFS.hpp>
#include <Services/Scheduler/Scheduler.hpp>
#include <Services/SysInitA/SysInitA.hpp>
#include <Services/ELF/KModule/KModule.hpp>
#include <Services/ELF/ELF.hpp>
#include <Services/Scheduler/Scheduler.hpp>

#include <HAL/MEM/PMEM.hpp>
#include <HAL/MEM/VMM.hpp>
#include <HAL/DISK/Disk.hpp>

using namespace HAL;
using namespace MEM;

namespace SysInitA {
    enum class IniSection {
        NONE,
        DRIVERS,
        APPLICATION
    };

    static inline void trim_line(char *line) {
        if (!line) return;
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\r' || line[len - 1] == '\n' || line[len - 1] == ' ' || line[len - 1] == '\t')) {
            line[len - 1] = '\0';
            len--;
        }
    }

    static char *duplicate_string(const char *src) {
        if (!src) return nullptr;
        size_t len = strlen(src);
        char *dest = new char[len + 1];
        if (!dest) return nullptr;
        memcpy(dest, src, len + 1);
        return dest;
    }

    void ParseBootConfig(const char *buffer, uint32_t length, lib::vec<const char *> &boot_drv, lib::vec<const char *> &boot_app) {
        IniSection current_section = IniSection::NONE;

        char *work_buf = new char[length + 1];
        if (!work_buf) return;

        memcpy(work_buf, buffer, length);
        work_buf[length] = '\0';

        char *saveptr = nullptr;
        char *line = strtok_r(work_buf, "\n", &saveptr);

        while (line != nullptr) {
            while (*line == ' ' || *line == '\t' || *line == '\r') {
                line++;
            }

            trim_line(line);

            if (*line == '\0' || *line == '#' || *line == ';') {
                line = strtok_r(nullptr, "\n", &saveptr);
                continue;
            }

            if (strcmp(line, "[DRIVERS]")) {
                current_section = IniSection::DRIVERS;
            } else if (strcmp(line, "[APPLICATION]")) {
                current_section = IniSection::APPLICATION;
            } else if (line[0] == '[' && line[strlen(line) - 1] == ']') {
                current_section = IniSection::NONE;
            } else {
                char *path_copy = duplicate_string(line);
                if (path_copy) {
                    if (current_section == IniSection::DRIVERS) {
                        (void)boot_drv.push_back(path_copy);
                    } else if (current_section == IniSection::APPLICATION) {
                        (void)boot_app.push_back(path_copy);
                    } else {
                        delete[] path_copy;
                    }
                }
            }

            line = strtok_r(nullptr, "\n", &saveptr);
        }

        delete[] work_buf;
    }

    constexpr const char *FIXED_LOAD_PATH = "A:/SYSTEM/BOOT.INI";
    void SpawnTasks(void *) {
        Debug::krnl_print("SYSA", Debug::LOG_INFO, "Initialize");
        lib::fullpath parsed_path = lib::parse_path(FIXED_LOAD_PATH);

        auto *root_disk = DISK::GetDisk(parsed_path.drv);
        lib::sptr<VFS::VNode> sys_list = root_disk->rootnode->resolve_path_to_vnode(parsed_path.path);
        
        uint32_t req_bytes = sys_list->get_size();
        
        if (req_bytes == 0) {
            return;
        }

        uint32_t req_pages = (req_bytes + (PAGE_SIZE - 1)) / PAGE_SIZE;

        uint8_t *byte_ptr = (uint8_t *)PMEM::alloc_pages(req_pages, VMM::PTE_PRESENT | VMM::PTE_WRITABLE);

        sys_list->read(0, byte_ptr, req_bytes);
        
        lib::vec<const char *> apps;
        lib::vec<const char *> drvrs;

        ParseBootConfig((const char *)byte_ptr, req_bytes, drvrs, apps);

        for (auto i{0uz}; i < drvrs.size(); ++i) {
            void *entry_point = ELF::KModule::load_module(drvrs[i]);

            if (entry_point) {
                new Scheduler::Task(
                    [](void *entry){
                        asm volatile("sti\ncall %0"
                            :
                            : "r"(entry)
                            : "memory"
                        );
                    },
                    drvrs[i],
                    true,
                    entry_point
                );
            }

            delete[] drvrs[i];
        }

        for (auto i{0uz}; i < apps.size(); ++i) {
            if (DISK::IsValidDisk(apps[i][0]))
                new Scheduler::Task([](void *path) {
                    char *p = (char *)path;
                    lib::string cp_path = p;
                    delete[] p;
                    Debug::krnl_print("SYSA", Debug::LOG_INFO, "Executing ring 3 task at: %s", cp_path.c_str());
                    ELF::Runway(cp_path);
                    for (;;);
                }, apps[i], true, (void *)apps[i]);
            else
                delete[] apps[i];
        }

        PMEM::free_pages(byte_ptr, req_pages);
        return;
    }
}