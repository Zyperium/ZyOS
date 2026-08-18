#include "Library/locks.hpp"
#include "Services/ELF/ELF.hpp"
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
#include <HAL/ACPI/ACPI.hpp>
#include <HAL/DISK/Disk.hpp>
#include <HAL/MSR.hpp>

#include <Services/Syscalls/Syscalls.hpp>
#include <Services/IPC/drvio.hpp>
#include <Services/Scheduler/Scheduler.hpp>

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


    lib::SoftLock syslock;

    /*
        @params usr_ptr: the LOWER half pointer to a VALID path.
        @returns a file descriptor.
    */
    uint64_t SYS_OPEN_FILE(uint64_t usr_ptr, uint64_t max) {
        // lib::ScopedSoftLock x(syslock);
        auto *val = usr_to_string(usr_ptr, max);

        Debug::krnl_print("SYS", Debug::LOG_INFO, "Received value %s", val);

        if (!val[0]) {
            delete[] val;
            return 0;
        }

        auto filed{0};

        auto *usr_task = HAL::CORE::get_core_data()->current_task;

        if (!usr_task->utask) {
            delete[] val;
            Debug::krnl_print("SYS", Debug::LOG_WARN, "Bad ring 3 task, or kernel task called a syscall function?");
            return 0;
        }

        filed = usr_task->utask->next_free_ds;

        if (filed > Scheduler::MAX_USR_FD) {
            usr_task->utask->next_free_ds = Scheduler::MAX_USR_FD + 1;
            for (auto i{0uz}; i < Scheduler::MAX_USR_FD; ++i) {
                if (!usr_task->utask->descriptors[i]) {
                    usr_task->utask->next_free_ds = i;
                    break;
                }
            }

            if (usr_task->utask->next_free_ds > Scheduler::MAX_USR_FD) {
                delete[] val;
                Debug::krnl_print("SYS", Debug::LOG_WARN, "Out of filedescriptors!");
                return 0;
            }

            filed = usr_task->utask->next_free_ds;
        }

        usr_task->utask->next_free_ds = Scheduler::MAX_USR_FD + 1;
        for (auto i{0uz}; i < Scheduler::MAX_USR_FD; ++i) {
            if (!usr_task->utask->descriptors[i]) {
                usr_task->utask->next_free_ds = i;
                break;
            }
        }

        auto p = lib::parse_path(val);
        delete[] val;

        if (!DISK::IsValidDisk(p.drv)) {
            Debug::krnl_print("SYS", Debug::LOG_WARN, "Received bad drive letter!");
            return 0;
        }

        Debug::krnl_print("SYS", Debug::LOG_INFO, "Resolving virtual node!");
        auto *node = DISK::GetRootOfDrive(p.drv)->resolve_path_to_vnode(p.path);

        if (!node) {
            Debug::krnl_print("SYS", Debug::LOG_WARN, "Received bad path!");
            return 0;
        }

        Debug::krnl_print("SYS", Debug::LOG_INFO, "Successfully opened file! (filed %i)", filed + 1);
        usr_task->utask->descriptors[filed] = node;

        return filed + 1;
    }

    /*
        @params file_descriptor: int value that points to the correlating file
        @params read_offset: current integer offset to read
        @params read_amount: how much from the offset to read
        @params buffer: the buffer to pass the file content to
        @returns bytes read
    */
    uint64_t SYS_READ_FILE(uint64_t file_descriptor, uint64_t read_offset, uint64_t read_amount, uint64_t buffer) {
        // lib::ScopedSoftLock x(syslock);
        auto *usr_task = HAL::CORE::get_core_data()->current_task;

        Debug::krnl_print("SYS", Debug::LOG_INFO, "Reading fd %i at %i to %i into %x", file_descriptor, read_offset, read_amount, buffer);

        if (!usr_task->utask->descriptors[file_descriptor - 1]) {
            Debug::krnl_print("SYS", Debug::LOG_WARN, "Invalid file descriptor");
            return 0;
        }

        auto *target = usr_task->utask->descriptors[file_descriptor - 1];
        
        if (read_offset >= target->get_size()) {
            Debug::krnl_print("SYS", Debug::LOG_WARN, "Offset >= than target size");
            return 0;
        }

        if (read_offset + read_amount > target->get_size()) {
            read_amount = target->get_size() - read_offset;
        }

        auto buf_phys = VMM::GetPhysicalAddress(usr_task->cr3, buffer);

        if (!buf_phys) {
            Debug::krnl_print("SYS", Debug::LOG_INFO, "Bad virtual address!");
            return 0;
        }

        target->read(read_offset, (void *)buffer, read_amount);

        Debug::krnl_print("SYS", Debug::LOG_INFO, "Read %i bytes", read_amount);

        return read_amount;
    }

    uint64_t SYS_WRITE_FILE(uint64_t file_descriptor, uint64_t write_offset, uint64_t write_amount, uint64_t buffer) {
        // lib::ScopedSoftLock x(syslock);
        auto *usr_task = HAL::CORE::get_core_data()->current_task;

        if (!usr_task->utask->descriptors[file_descriptor - 1]) {
            Debug::krnl_print("SYS", Debug::LOG_WARN, "Invalid file descriptor");
            return 0;
        }

        auto *target = usr_task->utask->descriptors[file_descriptor - 1];

        if (write_offset >= target->get_size()) {
            Debug::krnl_print("SYS", Debug::LOG_WARN, "Offset >= than target size");
            return 0;
        }

        auto buf_phys = VMM::GetPhysicalAddress(usr_task->cr3, buffer);

        if (!buf_phys) {
            Debug::krnl_print("SYS", Debug::LOG_INFO, "Bad virtual address!");
            return 0;
        }
        

        target->write(write_offset, (void *)buffer, write_amount);

        return write_amount;
    }

    uint64_t SYS_CLOSE_FILE(uint64_t fd) {
        // lib::ScopedSoftLock x(syslock);
        auto *usr_task = HAL::CORE::get_core_data()->current_task->utask;

        if (!usr_task->descriptors[fd]) {
            return 1; // invalid fd
        }

        usr_task->descriptors[fd]->release();
        usr_task->descriptors[fd] = nullptr;

        usr_task->next_free_ds = fd;

        return 0;
    }

    uint64_t SYS_KRNL_IO(uint64_t path, uint64_t data, uint64_t path_len, uint64_t extra) {
        // lib::ScopedSoftLock x(syslock);
        if (path_len > 32) path_len = 32;
        auto *val = usr_to_string(path, path_len);
        auto path_str = lib::string(val);

        auto **found = IPC::regdrvrs.find(path_str);

        if (!found) {
            Debug::krnl_print("SYS", Debug::LOG_WARN, "Unable to find ioctl");
            return -1;
        }

        auto *regdrvr = *found;

        if (!regdrvr->is_relier(HAL::CORE::get_core_data()->current_task))
            regdrvr->add_relier(HAL::CORE::get_core_data()->current_task);

        delete[] val;

        uint64_t mem_val = regdrvr->on_call(HAL::CORE::get_core_data()->current_task, data, extra);
        return mem_val;
    }

    uint64_t SYS_MMAP(uint64_t addr, uint64_t len, int prot, int flags, int fd, uint64_t off) {
        // lib::ScopedSoftLock x(syslock);
        if (len == 0) return -EINVAL;
        (void)fd;
        (void)off;

        uint64_t aligned_len = (len + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
        uint64_t pages = aligned_len / PAGE_SIZE;

        auto *usr_task = HAL::CORE::get_core_data()->current_task;
        uint64_t target = 0;

        if ((flags & MAP_FIXED) && addr != 0) {
            if (addr % PAGE_SIZE != 0) return -EINVAL;
            target = addr;
        } else {
            target = usr_task->utask->usr_virt_mmap;
            usr_task->utask->usr_virt_mmap += aligned_len;
        }

        uint64_t pte_flags = VMM::PTE_USER;
        if (prot != 0) pte_flags |= VMM::PTE_PRESENT;
        if (prot & 0x2) pte_flags |= VMM::PTE_WRITABLE;
        if (!(prot & 0x4)) pte_flags |= VMM::PTE_NX;

        for (size_t i = 0; i < pages; ++i) {
            uint64_t vaddr = target + (i * PAGE_SIZE);
            uint64_t phys_page = (uint64_t)PMM::alloc_page();

            if (!phys_page) return -ENOMEM;

            VMM::map_page(
                (uint64_t *)(usr_task->cr3 + PMM::hhdm_offset),
                vaddr,
                phys_page,
                pte_flags
            );
        }

        Debug::krnl_print("SYS", Debug::LOG_INFO, "Successfully mapped %x at %x", len, target);

        return target;
    }

    uint64_t SYS_MUNMAP(uint64_t addr, uint64_t len) {
        // lib::ScopedSoftLock x(syslock);
        if (addr % PAGE_SIZE != 0 || len == 0) return -EINVAL;

        auto *usr_task = HAL::CORE::get_core_data()->current_task;
        uint64_t aligned_len = (len + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
        uint64_t pages = aligned_len / PAGE_SIZE;

        for (size_t i = 0; i < pages; ++i) {
            uint64_t vaddr = addr + (i * PAGE_SIZE);
            uint64_t phys = VMM::GetPhysicalAddress(usr_task->cr3, vaddr);

            if (phys) {
                VMM::unmap_page((uint64_t *)(usr_task->cr3 + PMM::hhdm_offset), vaddr);
                PMM::free_page((void *)phys);
            }
        }

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

                return SYS_OPEN_FILE(regs.A1, regs.A2);
            }
            case SYSCALL_ID::SYS_READ: {
                return SYS_READ_FILE(regs.A1, regs.A2, regs.A3, regs.A4);
            }
            case SYSCALL_ID::SYS_GLEN: {
                auto *usr_task = HAL::CORE::get_core_data()->current_task;

                if (!usr_task->utask->descriptors[regs.A1 - 1]) {
                    Debug::krnl_print("SYS", Debug::LOG_WARN, "Invalid file descriptor");
                    return 0;
                }

                auto *target = usr_task->utask->descriptors[regs.A1 - 1];

                return target->get_size();
            }
            case SYSCALL_ID::SYS_WRITE: {
                return SYS_WRITE_FILE(regs.A1, regs.A2, regs.A3, regs.A4);
            }
            case SYSCALL_ID::SYS_CLOSE: {
                return SYS_CLOSE_FILE(regs.A1);
            }
            case SYSCALL_ID::SYS_IOCTL: {
                return SYS_KRNL_IO(regs.A1, regs.A2, regs.A3, regs.A4);
            }
            case SYSCALL_ID::SYS_LOUT: {
                char *x = usr_to_string(regs.A1, regs.A2);
                Debug::krnl_print(
                    "SYS", 
                    Debug::LOG_INFO, 
                    "%s: %s", 
                    HAL::CORE::get_core_data()->current_task->task_name.c_str(), 
                    x
                );
                delete[] x;
                return 0;
            }
            case SYSCALL_ID::SYS_MMAP: {
                return SYS_MMAP(regs.A1, regs.A2, regs.A3, regs.A4, regs.A5, regs.A6);
            }
            case SYSCALL_ID::SYS_MUNMAP: {
                return SYS_MUNMAP(regs.A1, regs.A2);
            }
            case SYSCALL_ID::SYS_MPROTECT: {
                Debug::krnl_print("SYS", Debug::LOG_WARN, "Unimplemented (mprotect)");
                return -1;
            }
            case SYSCALL_ID::SYS_EXIT: {
                Debug::krnl_print("SYS", Debug::LOG_INFO, "Task %s is exiting", HAL::CORE::get_core_data()->current_task->task_name.c_str());
                Scheduler::Suicide();
                return 0;
            }
            case SYSCALL_ID::SYS_YIELD: {
                Scheduler::Yield();
                return 0;
            }
            case SYSCALL_ID::SYS_FORK: {
                auto t = HAL::CORE::get_core_data()->current_task;
                uint64_t og_pid = HAL::CORE::get_core_data()->current_task->get_pid();

                t->block(Scheduler::BlockReasons::FORK);
                
                return og_pid;
            }
            case SYSCALL_ID::SYS_SET_FS_BASE: {
                if (regs.A1 >= ZyOS::END_OF_LOWER_HALF)
                    return -EINVAL;

                MSR::wrmsr(MSR::IA32_FS_BASE, regs.A1);
                return 0;
            }
            case SYSCALL_ID::SYS_SHM_CREATE: {
                Debug::krnl_print("SYS", Debug::LOG_WARN, "Unimplemented (shm create)");
                return -1;
            }
            case SYSCALL_ID::SYS_SHM_MAP: {
                Debug::krnl_print("SYS", Debug::LOG_WARN, "Unimplemented (shm map)");
                return -1;
            }
            case SYSCALL_ID::SYS_SHM_UNMAP: {
                Debug::krnl_print("SYS", Debug::LOG_WARN, "Unimplemented (shm unmap)");
                return -1;
            }
            case SYSCALL_ID::SYS_FUTEX_WAIT: {
                Debug::krnl_print("SYS", Debug::LOG_WARN, "Unimplemented (futex wait)");
                return -1;
            }
            case SYSCALL_ID::SYS_FUTEX_WAKE: {
                Debug::krnl_print("SYS", Debug::LOG_WARN, "Unimplemented (futex wake)");
                return -1;
            }
            case SYSCALL_ID::SYS_GET_TIME: {
                return ACPI::get_sys_time();
            }
            case SYSCALL_ID::SYS_GET_PID: {
                uint64_t pid = HAL::CORE::get_core_data()->current_task->get_pid();
                return pid;
            }
            case SYSCALL_ID::SYS_EXEC_APP: {
                char *path = usr_to_string(regs.A1, 64);

                Scheduler::Task *ntask = new Scheduler::Task([](void *ptr){ 
                    char stack[64];
                    memcpy(stack, ptr, 64);
                    delete[] (char *)ptr;
                    ELF::Runway(stack);
                    Scheduler::Suicide();
                }, path, true, path);
                
                asm volatile("mfence" ::: "memory");

                return ntask->get_pid();
            }
            default:
                Debug::krnl_print("SYS", Debug::LOG_WARN, "Unknown syscall ID! (%i)", id);
                break;
        }

        return -EINVAL;
    }
}

extern "C" uint64_t SysDisp(Syscalls::REGS registers) {
    Syscalls::SUBREGS s;
    memcpy(&s, &registers.A1, sizeof(Syscalls::SUBREGS));

    return Syscalls::HandleSyscall(static_cast<Syscalls::SYSCALL_ID>(registers.ID), s);
}