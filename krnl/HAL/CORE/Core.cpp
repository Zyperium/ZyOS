#include <HAL/CORE/Core.hpp>
#include <HAL/MSR.hpp>

namespace HAL::CORE {
    void init_core(ThreadLocal *data) {
        uint64_t addr = reinterpret_cast<uint64_t>(data);

        MSR::wrmsr(MSR::IA32_GS_BASE, addr);
        MSR::wrmsr(MSR::IA32_KERNEL_GS_BASE, addr);
    }

    size_t get_core_id() {
        size_t core_id;
        
        __asm__ (
            "movq %%gs:16, %0"
            : "=r" (core_id)
            :                   
            :                   
        );

        return core_id;
    }
}