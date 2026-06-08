#include <HAL/CORE/Core.hpp>
#include <HAL/MSR.hpp>

namespace HAL::CORE {
    void init_core(ThreadLocal *data) {
        uint64_t addr = reinterpret_cast<uint64_t>(data);

        MSR::wrmsr(MSR::IA32_GS_BASE, addr);
        MSR::wrmsr(MSR::IA32_KERNEL_GS_BASE, addr);
    }

    ThreadLocal *get_thread_data() {
        ThreadLocal *core_id;
        
        __asm__ (
            "movq %%gs:0, %0"
            : "=r" (core_id)
            :
            :         
        );

        return core_id;
    }
}