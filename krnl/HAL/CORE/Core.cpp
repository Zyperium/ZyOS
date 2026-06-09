#include <HAL/IDT/IDT.hpp>
#include <Library/io.hpp>
#include <Library/debug.hpp>
#include <Library/regs.h>
#include <HAL/CORE/Core.hpp>
#include <HAL/MEM/VMM.hpp>
#include <HAL/MEM/PMM.hpp>
#include <HAL/MSR.hpp>
#include <stdint.h>
#include <limine.h>

static constinit volatile limine_mp_request mp_request = {
    .id = LIMINE_MP_REQUEST_ID, 
    .revision = 0,
    .response = nullptr,
    .flags = 0
};

namespace HAL::CORE {
    void init_core(ThreadLocal *data) {
        uint64_t addr = reinterpret_cast<uint64_t>(data);

        MSR::wrmsr(MSR::IA32_GS_BASE, addr);
        MSR::wrmsr(MSR::IA32_KERNEL_GS_BASE, addr);

        init_lapic();
    }

    void addi_core_EP() {
        
    }

    void discover_all_cores() {
        limine_mp_response *mp_resp = mp_request.response;
        uint64_t total_cores = mp_resp->cpu_count;
        uint32_t bsp_lapic_id = mp_resp->bsp_lapic_id;

        for (auto i{0uz}; i < total_cores; i++) {
            limine_mp_info *core = mp_resp->cpus[i];

            if (core->lapic_id == bsp_lapic_id) {
                continue;
            }

            __atomic_store_n(&core->goto_address, (limine_goto_address)addi_core_EP, __ATOMIC_SEQ_CST);
        }
    }

    uintptr_t lapic_base_ptr = 0;
    void(*idleptr)() = nullptr;

    ThreadLocal *get_thread_data() {
        ThreadLocal *core_id{nullptr};
        
        __asm__ (
            "movq %%gs:0, %0"
            : "=r" (core_id)
            :
            :
        );

        if (!core_id) {
            Debug::krnl_print("CORE", Debug::LOG_INFO, "Received nullptr from GS register. (Reading from user?)");
        }

        return core_id;
    }

    void calibrate_lapic() {
        lapic_write(LAPIC_TIMER_DIV, LAPIC_TIMER_DIV);

        outb(PIT::PORT_COMMAND, PIT::CONFIG_CALIBRATION_MODE);
        outb(PIT::PORT_CHANNEL0, PIT::RELOAD_VALUE & 0xFF);
        outb(PIT::PORT_CHANNEL0, (PIT::RELOAD_VALUE >> 8) & 0xFF);

        lapic_write(LAPIC_TIMER_INITCNT, LAPIC_INIT_COUNT);

        uint16_t pit_count{};

        do {
            outb(PIT::PORT_COMMAND, PIT::CONFIG_LATCH_COMMAND);
            uint8_t lo = inb(PIT::PORT_CHANNEL0);
            uint8_t hi = inb(PIT::PORT_CHANNEL0);
            pit_count = (hi << 8) | lo;
        } while (pit_count > 0 && pit_count <= PIT::RELOAD_VALUE);

        uint32_t scheduler_timer_mode = LAPIC_PERODIC_MODE | IDT::LAPIC_VECTOR;

        lapic_write(LAPIC_LVT_TIMER, scheduler_timer_mode);

        uint32_t ticks_in_10ms = LAPIC_INIT_COUNT - lapic_read(LAPIC_TIMER_CURCNT);
        uint32_t ticks_in_1ms  = ticks_in_10ms / PIT::TARGET_PERIOD_MS;

        get_thread_data()->lapic_ticks_per_ms = ticks_in_1ms;
        lapic_write(LAPIC_TIMER_INITCNT, ticks_in_1ms);
        lapic_write(LAPIC_PRIORITY_REG, 0);
        Debug::krnl_print("CORE", Debug::LOG_INFO, "Calibrated lapic @ %i ticks_per_ms", ticks_in_1ms);
    }

    bool lapic_mapped{false};
    void init_lapic() {
        if (!lapic_mapped) {
            uintptr_t lapic_addr = MSR::rdmsr(MSR::IA32_APIC_BASE) & LAPIC_ADDR_MASK;
            lapic_base_ptr = lapic_addr + MEM::PMM::hhdm_offset;

            MEM::VMM::map_page(reinterpret_cast<uint64_t *>(read_cr3() & MEM::VMM::PTE_ADDR_MASK), lapic_base_ptr, lapic_addr, MEM::VMM::PTE_PRESENT | MEM::VMM::PTE_WRITABLE | MEM::VMM::PTE_CACHELESS);
            lapic_mapped = true;
        }

        Debug::krnl_print("CORE", Debug::LOG_INFO, "Initializing LAPIC on core %i", get_thread_data()->core_id);
        uint32_t spurious_reg = lapic_read(LAPIC_SPURIOUS);

        spurious_reg &= ~LAPIC_SPURIOUS_VECTOR_MASK;
        spurious_reg |= LAPIC_APIC_SOFTWARE_ENABLE | IDT::LAPIC_VECTOR;
        lapic_write(LAPIC_SPURIOUS, spurious_reg);
        Debug::krnl_print("CORE", Debug::LOG_INFO, "Lapic initialized!");

        calibrate_lapic();

        lapic_write(LAPIC_TIMER_DIV, 0x0);
    }
    
    void ack_lapic() {
        lapic_write(LAPIC_EOI, 0);
        return;
    }
}