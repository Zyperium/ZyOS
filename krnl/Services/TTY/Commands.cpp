#include <Services/Scheduler/Scheduler.hpp>
#include <Services/TTY/Commands.hpp>
#include <Services/TTY/TTY.hpp>
#include <Services/ELF/KModule/KModule.hpp>
#include <Services/ELF/ELF.hpp>

#include <Library/debug.hpp>
#include <Library/string.h>
#include <Library/krnlptr.hpp>
#include <Library/path.hpp>

#include <Services/VFS/VFS.hpp>
#include <Services/VFS/FAT32/FAT32.hpp>

#include <HAL/DISK/Disk.hpp>
#include <HAL/PCI/xHCI/msix_xhci.hpp>
#include <HAL/CORE/Core.hpp>

#include <stdint.h>
#include <stddef.h>

namespace TTY::Commands {
    constexpr uCMD vfs_commands[] {
        { _hash("echo"), PROC::echo_processor},
        { _hash("ls"), PROC::ls_processor},
        {_hash("swapd"), PROC::swapd_processor},
        {_hash("cd"), PROC::cd_processor},
        {_hash("touch"), PROC::touch_processor},
        {_hash("cat"), PROC::cat_processor},
        {_hash("clear"), PROC::clear_processor},
        {_hash("sysload"), PROC::driver_processor},
        {_hash("zyx"), PROC::execute_processor},
        {_hash("watchpid"), PROC::watch_processor},
        {_hash("whatpid"), PROC::whatpid_processor},
        {_hash("fault"), PROC::fault_processor},
        {_hash("nproc"), PROC::nproc_processor}
    };

    constexpr size_t vfs_commands_length = sizeof(vfs_commands) / sizeof(vfs_commands[0]);

    constexpr const char *ERR_MSG = "Unknown command!\n";
    lib::string evaluate_cmd(const char *cmd) {
        size_t cmd_len = strlen(cmd);
        char *cmd_copy = new char[cmd_len + 1];
        strcpy(cmd_copy, cmd);

        int argc = 0;
        char *ptr = cmd_copy;
        bool in_token = false;

        while (*ptr != '\0') {
            if (*ptr != ' ' && !in_token) {
                in_token = true;
                argc++;
            } else if (*ptr == ' ') {
                in_token = false;
            }
            ptr++;
        }

        if (argc == 0) {
            delete[] cmd_copy;
            return ERR_MSG;
        }

        char **argv = new char*[argc + 1];
        ptr = cmd_copy;
        int current_arg = 0;
        in_token = false;

        while (*ptr != '\0') {
            if (*ptr != ' ') {
                if (!in_token) {
                    argv[current_arg++] = ptr;
                    in_token = true;
                }
            } else {
                if (in_token) {
                    *ptr = '\0';
                    in_token = false;
                }
            }
            ptr++;
        }

        argv[argc] = nullptr;

        uint32_t hash = _hash(argv[0]);

        for (auto i{0uz}; i < vfs_commands_length; i++) {
            if (hash != vfs_commands[i].hash) 
                continue; 

            Debug::krnl_print("CMD", Debug::LOG_INFO, "Matched command");

            lib::string response = vfs_commands[i].invoke(argc, argv);

            delete[] argv;
            delete[] cmd_copy; 
            return response;
        }

        delete[] argv;
        delete[] cmd_copy; 

        Debug::krnl_print("CMD", Debug::LOG_INFO, "Unknown command (%s)", cmd);

        return ERR_MSG;
    }

    namespace PROC {
        char *get_abs_path(lib::string current_path) {
            auto make_err = []() { return new char[2]{'X', '\0'}; };

            if (current_path.length() < 3)
                return make_err();

            auto *cur_host = conhosts[active_host];

            if (current_path[0] == '/') {
                if (current_path[2] != '/')
                    return make_err();
            
                char *ptr = new char[current_path.length() + 1]{0}; 
                memcpy(ptr, current_path.c_str(), current_path.length());
                ptr[0] = current_path[1];
                ptr[1] = ':';
                return ptr;
            }
        
            if (current_path[1] == ':') {
                char *ptr = new char[current_path.length() + 1]{0};
                memcpy(ptr, current_path.c_str(), current_path.length());
                return ptr;
            }
        
            bool needs_slash = (cur_host->current_wd.empty() || cur_host->current_wd[cur_host->current_wd.length() - 1] != '/');

            size_t total_len = 2 + cur_host->current_wd.length() + (needs_slash ? 1 : 0) + current_path.length();

            char *ptr = new char[total_len + 1]{0};

            ptr[0] = cur_host->_ltrdrive;
            ptr[1] = ':';

            size_t offset = 2;
            memcpy(ptr + offset, cur_host->current_wd.c_str(), cur_host->current_wd.length());
            offset += cur_host->current_wd.length();

            if (needs_slash) {
                ptr[offset] = '/';
                offset += 1;
            }

            memcpy(ptr + offset, current_path.c_str(), current_path.length());

            return ptr;
        }

        lib::string echo_processor(int argc, char **argv) {
            if (argc < 2) {
                return "";
            }

            lib::string s;
            for (auto i{1}; i < argc; ++i) {
                s += argv[i];
                s += " ";
            }
            --s;
            s += "\n";
            return s;
        }

        lib::string swapd_processor(int argc, char **argv) {
            if (argc < 2) {
                return "Warning: No drive letter specified.\n";
            }

            if (argc > 3) {
                return "Warning: Too many arguments!\n";
            }

            if (strlen(argv[1]) != 1) {
                return "Warning: you must pass a letter!\n";
            }

            if (!HAL::DISK::IsValidDisk(argv[1][0])) {
                return "Warning: Invalid disk letter!\n";
            }

            conhosts[active_host]->current_wd = "/";
            conhosts[active_host]->_ltrdrive = argv[1][0];
            return "\n";
        }

        lib::string ls_processor(int argc, char **argv) {
            ConHost *cur_host = conhosts[active_host];

            Scheduler::Yield();

            if (cur_host->_ltrdrive == '?') {
                return "Warning: You are currently not on a valid drive.\n";
            }

            char *abs_path{nullptr};
            if (argc > 1) {
                abs_path = get_abs_path(argv[1]);
                Debug::krnl_print("CMD", Debug::LOG_INFO, "Val %s", abs_path);
                if (abs_path[0] == 'X') {
                    delete[] abs_path;
                    return "Warning: invalid path";
                }
            }
            else {
                abs_path = new char[cur_host->current_wd.length() + 2]{0};
                abs_path[0] = cur_host->_ltrdrive;
                abs_path[1] = ':';
                memcpy(&abs_path[2], cur_host->current_wd.c_str(), cur_host->current_wd.length());
            }

            VFS::VNode *root_n = HAL::DISK::GetRootOfDrive(abs_path[0]);

            if (!root_n) {
                delete[] abs_path;
                return "Error: Unable to reach root node of drive! (-1)\n";
            }

            VFS::VNode *target_directory = root_n->resolve_path_to_vnode(cur_host->current_wd);
            Debug::krnl_print("VFS", Debug::LOG_INFO, "Target Directory pointer is: %x", target_directory);

            if (target_directory == (VFS::VNode *)0xCCCCCCCCCCCCCCCC) {
                delete[] abs_path;
                return "Critical Error: Poisioned value as target_directory!\n";
            }

            if (!target_directory) {
                delete[] abs_path;
                return "Warning: Non-existant directory!\n";
            }

            lib::string result = "Reading directory: ";
            result += cur_host->current_wd.c_str();
            result += "\nName      | Size      |\n";
            uint64_t byte_offset = 0;
            VFS::FAT32::DirectoryEntry entry;

            while (target_directory->read(byte_offset, &entry, sizeof(VFS::FAT32::DirectoryEntry))) {
                Debug::krnl_print("CMD", Debug::LOG_INFO, "Read directory entry!");
                byte_offset += sizeof(VFS::FAT32::DirectoryEntry);
                uint8_t initial_byte = static_cast<uint8_t>(entry.name[0]);
                if (initial_byte == VFS::FAT32::DIR_ENTRY_FREE_ONWARD) {                    
                    break; 
                }
                if (initial_byte == VFS::FAT32::DIR_ENTRY_FREE) {
                    Debug::krnl_print("CMD", Debug::LOG_INFO, "Dir entry free!");
                    continue; 
                }
                if (entry.attr == VFS::FAT32::ATTR_LONG_NAME) {
                    Debug::krnl_print("CMD", Debug::LOG_INFO, "Long name attribute!");
                    continue; 
                }

                char clean_filename[13]; 
                int name_length = 0;

                for (int i = 0; i < 8; ++i) {
                    if (entry.name[i] != ' ') {
                        clean_filename[name_length++] = entry.name[i];
                    }
                }

                if (entry.name[8] != ' ' || entry.name[9] != ' ' || entry.name[10] != ' ') {
                    clean_filename[name_length++] = '.';
                    for (int i = 8; i < 11; ++i) {
                        if (entry.name[i] != ' ') {
                            clean_filename[name_length++] = entry.name[i];
                        }
                    }
                }
                clean_filename[name_length] = '\0';
                if (entry.attr & VFS::FAT32::ATTR_DIRECTORY) {
                    result += clean_filename;
                    result += "/";
                    result += "\n";
                } else {
                    result += clean_filename;
                    char tmp_buf[32];
                    Debug::snprintf(tmp_buf, 32, "(%i bytes)\n", entry.file_size);
                    result += tmp_buf;
                }
            }

            if (target_directory != root_n) {
                delete target_directory;
            }

            delete[] abs_path;
            return result;
        }

        lib::string cd_processor(int argc, char **argv) {
            if (argc < 2) return "";
            ConHost *cur_host = conhosts[active_host];

            if (strcmp(argv[1], ".")) {
                Debug::krnl_print("CMD", Debug::LOG_INFO, "Current directory (unchanging)");
                return "";
            }

            if (strcmp(argv[1], "..")) {
                Debug::krnl_print("CMD", Debug::LOG_INFO, "Matched change dir ..");
                if (cur_host->current_wd.length() == 1) {
                    Debug::krnl_print("CMD", Debug::LOG_INFO, "Currently at the root vnode");
                    return "";
                }

                while (cur_host->current_wd[cur_host->current_wd.length() - 1] != '/') {
                    --cur_host->current_wd;
                }

                if (cur_host->current_wd.length() != 1)
                    --cur_host->current_wd;

                return "";
            }

            lib::sptr<VFS::VNode> root_n = HAL::DISK::GetRootOfDrive(cur_host->_ltrdrive);

            lib::string t_path = cur_host->current_wd;
            if (t_path[cur_host->current_wd.length() - 1] != '/')
                t_path += "/";
            t_path += argv[1];
            if (!root_n->resolve_path_to_vnode(t_path)) {
                return "No such file or directory\n";
            }

            cur_host->current_wd = t_path;

            return "";
        }

        lib::string touch_processor(int argc, char **argv) {
            if (argc < 2) {
                return "Warning: Not enough arguments! (-h for help)\n";
            }

            ConHost *cur_host = conhosts[active_host];
            if (argv[1][0] == '/') {
                return "Created file!\n";
            }
        
            lib::string combined = cur_host->current_wd.c_str();
            combined += "/";
            combined += argv[1];

            VFS::VNode *root = HAL::DISK::GetRootOfDrive(cur_host->_ltrdrive);
            lib::sptr<VFS::VNode> node = root->resolve_path_to_vnode(combined);

            if (node) {
                return "File already exists! (-1)\n";
            }

            int last_slash_idx = -1;
            for (int i = 0; i < (int)combined.length(); ++i) {
                if (combined[i] == '/') {
                    last_slash_idx = i;
                }
            }

            lib::string directory_path = "";
            lib::string filename = "";

            if (last_slash_idx == 0) {
                directory_path = "/";
            } else {
                for (int i = 0; i < last_slash_idx; ++i) {
                    directory_path += combined[i];
                }
            }

            for (int i = last_slash_idx + 1; i < (int)combined.length(); ++i) {
                filename += combined[i];
            }

            lib::sptr<VFS::VNode> new_file_directory = root->resolve_path_to_vnode(directory_path);
            if (!new_file_directory) {
                return "Error: Parent directory not found!\n";
            }

            lib::sptr<VFS::VNode> new_file = new_file_directory->create(filename.c_str(), VFS::FileType::Regular);

            return "Created file!\n";
        }

        lib::string cat_processor(int argc, char **argv) {
            if (argc < 2) {
                return "Warning: Not enough arguments!\n";
            }
        
            ConHost *cur_host = conhosts[active_host];
        
            VFS::VNode *root_n = HAL::DISK::GetRootOfDrive(cur_host->_ltrdrive);
        
            if (argv[1][0] == '/') {
                return "";
            }
        
            lib::string t_path = cur_host->current_wd.c_str();
            if (t_path.length() > 1)
                t_path += "/";
            t_path += argv[1];
        
            lib::sptr<VFS::VNode> node = root_n->resolve_path_to_vnode(t_path);
        
            if (!node) {
                return "Warning: No such file or directory!\n";
            }
        
            if (node->get_type() == VFS::FileType::Directory) {
                return "Warning: is directory\n";
            }
            else if (node->get_size() == 0) {
                return "";
            }
        
            char *buffer = new char[node->get_size() + 1];
            uint64_t bytes_read = node->read(0, buffer, node->get_size());
            buffer[bytes_read] = '\0';
        
            lib::string current_line = "";
            for (uint64_t i = 0; i < bytes_read; ++i) {
                if (buffer[i] == '\n') {
                    current_line += "\n"; 
                    cur_host->draw_string(current_line.c_str(), HAL::SCREEN::COL::WHITE);
                    current_line = "";
                } else {
                    current_line += buffer[i];
                }
            }
        
            if (current_line.length() > 0) {
                current_line += "\n";
                cur_host->draw_string(current_line.c_str(), HAL::SCREEN::COL::WHITE);
            }
        
            delete[] buffer;
            return "";
        }

        lib::string clear_processor(int, char **) {
            conhosts[active_host]->reset_view();
            return "";
        }

        lib::string driver_processor(int argc, char **argv) {
            if (argc < 2) {
                return "Warning: You must pass a path to the driver!\n";
            }

            char *pathptr = get_abs_path(argv[1]);
            Debug::krnl_print("CMD", Debug::LOG_INFO, "Abs path is %s", pathptr);
            void *entry_point = ELF::KModule::load_module(pathptr);
            delete[] pathptr;

            if (!entry_point) {
                return "Bad module, or a library module!\n";
            }

            new Scheduler::Task([](void *entry) {
                Debug::krnl_print("CMD", Debug::LOG_INFO, "Executing entry target at: %x", entry);
                asm volatile(
                    "call %0"
                    :
                    : "r"(entry)
                    : "memory"
                );
                Debug::krnl_print("CMD", Debug::LOG_INFO, "Driver finished execution");
                Scheduler::Suicide();
                for (;;);
            }, "Kernel Module", true, entry_point);

            Scheduler::Yield();
            return "Module Loaded!\n";
        }

        lib::string execute_processor(int argc, char **argv) {
            if (argc < 2) {
                return "You must pass a path to load\n";
            }

            size_t alloc_size = strlen(argv[1]) + 1;
            char *k_buf = new char[alloc_size];
            strcpy(k_buf, argv[1]);
            k_buf[alloc_size - 1] = 0;
            Scheduler::Task *t = new Scheduler::Task([](void *path) {
                char *p = (char *)path;
                lib::string cp_path = p;
                delete p;
                Debug::krnl_print("CMD", Debug::LOG_INFO, "Executing ring 3 task at: %s", cp_path.c_str());
                ELF::Runway(cp_path);
                for (;;);
            }, "Usr Task", true, k_buf);

            Debug::krnl_print("CMD", Debug::LOG_INFO, "Task has id %i, path %s", t->get_pid(), k_buf);

            Scheduler::Yield();
            return "User process loaded!\n";
        }

        lib::string watch_processor(int argc, char **argv) {
            if (argc < 2) {
                return "You must pass a PID to watch\n";
            }

            Scheduler::watch_pid = TTY::kernel_atoi(argv[1]);

            char response[32];
            Debug::snprintf(response, 32, "Watching pid %i\n", TTY::kernel_atoi(argv[1]));
            return response;
        }

        lib::string whatpid_processor(int argc, char **argv) {
            if (argc < 2) {
                return "You must pass a PID to read\n";
            }

            Scheduler::Task *t_pid = Scheduler::GetTaskByPID(TTY::kernel_atoi(argv[1]));

            if (!t_pid) {
                return "Non-existant PID\n";
            }

            char response[96];
            Debug::krnl_print("CMD", Debug::LOG_INFO, "PID name %s. vruntime: %i. Last ran %ims ago.", t_pid->task_name.c_str(), t_pid->vruntime, Scheduler::LastRunTime(t_pid));
            Debug::snprintf(response, 96, "PID name %s. vruntime: %i. Last ran %ims ago.\n", t_pid->task_name.c_str(), t_pid->vruntime, Scheduler::LastRunTime(t_pid));
            return response;
        }

        lib::string fault_processor(int argc, char **argv) {
            if (argc < 2) {
                return "You must pass a fault type (page, gp, div0, triple)\n";
            }

            if (strcmp(argv[1], "page")) {
                auto *ptr = (int *)0x0;
                *ptr = 0xDEAD;
            }
            else if (strcmp(argv[1], "general")) {
                auto *ptr = (int *)0x32132143324324;
                *ptr = 0xDEAD;
            }
            else if (strcmp(argv[1], "div0")) {
                asm volatile(
                    "mov $0x0, %%rax;"
                    "mov $0x0, %%rbx;"
                    "xor %%rdx, %%rdx;"
                    "div %%rbx;"
                    :
                    :
                    : "rax", "rbx", "rdx"
                );
            }
            else if (strcmp(argv[1], "triple")) {
                asm volatile(
                    "push %q0\n\t"
                    "push %q1\n\t"
                    "lidt 0(%%rsp)\n\t"
                    "add $16, %%rsp"
                    :
                    : "r" (0), "r" (0)
                    : "memory"
                );
            }

            return "Done!\n";
        }

        lib::string nproc_processor(int, char **) {
            char ret_da[16];

            Debug::snprintf(ret_da, 64, "%i\n", HAL::CORE::get_core_count());
            return ret_da;
        }
    }
}