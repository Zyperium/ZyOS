#include <TTY/Commands.hpp>
#include <TTY/TTY.hpp>

#include <Library/debug.hpp>
#include <Library/string.h>

#include <VFS/VFS.hpp>
#include <VFS/FAT32/FAT32.hpp>

#include <HAL/DISK/Disk.hpp>
#include <HAL/PCI/xHCI/msix_xhci.hpp>

#include <stdint.h>
#include <stddef.h>

namespace TTY::Commands {
    constexpr uint32_t _hash(const char* str) {
        uint32_t hash = FNV_OFFSET;
        while (*str) {
            hash ^= static_cast<uint32_t>(*str);
            hash *= FNV_PRIME;
            str++;
        }
        return hash;
    }


    constexpr uCMD vfs_commands[] {
        { _hash("echo"), PROC::echo_processor},
        { _hash("ls"), PROC::ls_processor},
        {_hash("swapd"), PROC::swapd_processor}
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

        return ERR_MSG;
    }

    namespace PROC {
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
                return "Warning: No drive letter specified.";
            }

            if (argc > 3) {
                return "Warning: Too many arguments!";
            }

            if (strlen(argv[1]) != 1) {
                return "Warning: you must pass a letter!";
            }

            conhosts[active_host]->current_wd = "/";
            conhosts[active_host]->_ltrdrive = argv[1][0];
            return "\n";
        }

        lib::string ls_processor(int argc, char **argv) {
            ConHost *cur_host = conhosts[active_host];

            (void)argc;
            (void)argv;

            Scheduler::Yield();

            if (cur_host->_ltrdrive == '?') {
                return "Warning: You are currently not on a valid drive.\n";
            }

            VFS::VNode *root_n = HAL::DISK::GetRootOfDrive(cur_host->_ltrdrive);

            if (!root_n) {
                return "Error: Unable to reach root node of drive! (-1)\n";
            }

            VFS::VNode *target_directory = root_n->resolve_path_to_vnode(cur_host->current_wd);
            Debug::krnl_print("VFS", Debug::LOG_INFO, "Target Directory pointer is: %x", target_directory);

            if (target_directory == (VFS::VNode *)0xCCCCCCCCCCCCCCCC) {
                return "Critical Error: Poisioned value as target_directory!\n";
            }

            if (!target_directory) {
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
                    Debug::krnl_print("CMD", Debug::LOG_WARN, 
                        "Hit FREE_ONWARD. Raw Entry: %x %x %x %x | Attr: %x | Size: %i", 
                        static_cast<uint8_t>(entry.name[0]), 
                        static_cast<uint8_t>(entry.name[1]), 
                        static_cast<uint8_t>(entry.name[2]), 
                        static_cast<uint8_t>(entry.name[3]),
                        entry.attr,
                        entry.file_size);

                    Debug::krnl_print("CMD", Debug::LOG_INFO, "Performing test read of LBA 0");
                    uint8_t tmp_buf[512];

                    HAL::DISK::Disk *d = HAL::DISK::GetDisk(cur_host->_ltrdrive);
                    d->dev->read(0, 1, tmp_buf);
                    
                    Debug::krnl_print("CMD", Debug::LOG_INFO, "First 4 bytes are: %x %x %x %x %x", tmp_buf[0], tmp_buf[1], tmp_buf[2], tmp_buf[3], tmp_buf[4]);
                    
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

            return result;
        }
    }
}