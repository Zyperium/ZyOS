#include <Library/debug.hpp>

#include <HAL/MEM/KMEM.hpp>
#include <HAL/MEM/VMM.hpp>
#include <HAL/MEM/PMM.hpp>
#include <HAL/MSR.hpp>

#include <Services/Syscalls/Syscalls.hpp>


extern "C" uint64_t SysDisp(Syscalls::REGS) {
    Debug::krnl_print("SYS", Debug::LOG_INFO, "Syscall!");
    return 0;
}

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
}