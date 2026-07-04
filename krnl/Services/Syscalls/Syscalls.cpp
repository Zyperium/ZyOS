#include <Library/debug.hpp>
#include <Library/string.h>
#include <Library/path.hpp>
#include <Library/cystr.hpp>
#include <Library/ZyOS.hpp>
#include <Library/krnlptr.hpp>

#include <HAL/MEM/KMEM.hpp>
#include <HAL/MEM/VMM.hpp>
#include <HAL/MEM/PMM.hpp>
#include <HAL/CORE/Core.hpp>
#include <HAL/DISK/Disk.hpp>
#include <HAL/MSR.hpp>

#include <Services/Syscalls/Syscalls.hpp>

using namespace HAL;
using namespace MEM;

namespace Syscalls {
    void initialize() {
        Debug::krnl_print("SYS", Debug::LOG_INFO, "Initialize");
        uint64_t efer = MSR::rdmsr(MSR::IA32_EFER);
        efer |= 1;

        MSR::wrmsr(MSR::IA32_EFER, efer);

        MSR::wrmsr(MSR::IA32_STAR, MSR_STAR_VAL);
        MSR::wrmsr(MSR::IA32_LSTAR, (uint64_t)&SysEntry);
        MSR::wrmsr(MSR::IA32_FMASK, MSR_FSTAR_VAL);

        return;
    }

    // inline static bool is_canon(uint64_t ptr) {
    //     if (ptr < ZyOS::END_OF_LOWER_HALF)
    //         return true;
    //     else if (ptr > ZyOS::START_OF_UPPER_HALF)
    //         return true;

    //     return false;
    // }

    char *usr_to_string(uint64_t usr_ptr, uint64_t max_value) {
        auto *a = new char[max_value + 1]{0};
        if (usr_ptr > ZyOS::END_OF_LOWER_HALF) {
            return a;
        }

        uint64_t phys_addr = VMM::GetPhysicalAddress(CORE::get_core_data()->current_task->cr3, usr_ptr);

        if (!phys_addr) {
            return a;
        }

        strncpy(a, (const char *)(phys_addr + PMM::hhdm_offset), max_value);
        
        return a;
    }

    /*
        @params usr_ptr: the LOWER half pointer to a VALID path.
        @returns a file descriptor.
    */
    uint64_t SYS_OPEN_FILE(uint64_t usr_ptr, uint64_t max) {
        char *val = usr_to_string(usr_ptr, max);

        Debug::krnl_print("SYS", Debug::LOG_INFO, "Received value %s", val);

        if (!val[0]) {
            delete[] val;
            return 0;
        }

        auto filed{0};

        Scheduler::Task *usr_task = HAL::CORE::get_core_data()->current_task;

        if (!usr_task->utask) {
            delete[] val;
            Debug::krnl_print("SYS", Debug::LOG_WARN, "Bad ring 3 task, or kernel task called a syscall function?");
            return 0;
        }

        filed = usr_task->utask->next_free_ds;

        if (filed > Scheduler::MAX_USR_FD) {
            delete[] val;
            Debug::krnl_print("SYS", Debug::LOG_WARN, "Out of filedescriptors!");
            return 0;
        }

        usr_task->utask->next_free_ds = Scheduler::MAX_USR_FD;
        for (auto i{0uz}; i < Scheduler::MAX_USR_FD; ++i) {
            if (!usr_task->utask->descriptors[i]) {
                usr_task->utask->next_free_ds = i;
                break;
            }
        }

        lib::fullpath p = lib::parse_path(val);
        delete[] val;

        if (!DISK::IsValidDisk(p.drv)) {
            Debug::krnl_print("SYS", Debug::LOG_WARN, "Received bad drive letter!");
            return 0;
        }

        VFS::VNode *node = DISK::GetRootOfDrive(p.drv)->resolve_path_to_vnode(p.path);

        if (!node) {
            Debug::krnl_print("SYS", Debug::LOG_WARN, "Received bad path!");
            return 0;
        }

        Debug::krnl_print("SYS", Debug::LOG_INFO, "Successfully opened file! (filed %i)", filed + 1);
        usr_task->utask->descriptors[filed] = node;

        return filed + 1;
    }

    /*
    
    */
    uint64_t SYS_READ_FILE(uint64_t file_descriptor, uint64_t read_offset, uint64_t read_amount) {
        (void)file_descriptor;
        (void)read_offset;
        (void)read_amount;
        return 0;
    }

    uint64_t HandleSyscall(SYSCALL_ID id, SUBREGS regs) {

        switch(id) {
            /*
                Expects A1 to contain a user address that is either:
                1) A standard letter->path setup
                OR
                2) A mingw esque slash setup
            */
            case SYSCALL_ID::SYS_OPEN: {
                if (!regs.A1) {
                    return -1;
                }

                Debug::krnl_print("SYS", Debug::LOG_INFO, "Open file syscall (usr address: %x, usr size: %i)", regs.A1, regs.A2);
                
                return SYS_OPEN_FILE(regs.A1, regs.A2);
            }
            case SYSCALL_ID::SYS_READ: {
                Debug::krnl_print("SYS", Debug::LOG_INFO, "Reading file %i", regs.A1);
                return SYS_READ_FILE(regs.A1, regs.A2, regs.A3);
            }
            case SYSCALL_ID::SYS_WRITE: {
                break;
            }
            case SYSCALL_ID::SYS_CLOSE: {
                break;
            }
            default:
                Debug::krnl_print("SYS", Debug::LOG_WARN, "Unknown syscall ID!");
                break;
        }

        return -1;
    }
}

extern "C" uint64_t SysDisp(Syscalls::REGS registers) {
    Syscalls::SUBREGS s;
    memcpy(&s, &registers.A1, sizeof(Syscalls::SUBREGS));

    return Syscalls::HandleSyscall(static_cast<Syscalls::SYSCALL_ID>(registers.ID), s);
}