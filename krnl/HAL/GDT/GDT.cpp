#include <Library/debug.hpp>
#include <HAL/GDT/GDT.hpp>
#include <Library/string.h>

namespace HAL::GDT {
    TSS tss;
    GDT gdt_table;
    static uint8_t double_fault_stack[8192] __attribute__((aligned(16)));
    static uint8_t timer_stack[8192] __attribute__((aligned(16)));
    static uint8_t nmi_stack[8192] __attribute__((aligned(16)));

    void SetEntry(GDTEntry* entry, uint64_t base, uint32_t limit, uint8_t access, uint8_t gran) {
        entry->base_low = static_cast<uint16_t>(base & 0xFFFF);
        entry->base_middle = static_cast<uint8_t>((base >> 16) & 0xFF);
        entry->base_high = static_cast<uint8_t>((base >> 24) & 0xFF);

        entry->limit_low = static_cast<uint16_t>(limit & 0xFFFF);

        entry->access = access;
        entry->granularity = static_cast<uint8_t>(((limit >> 16) & 0x0F) | (gran & 0xF0));
    }

    void SetTSSEntry(GDTEntryTSS* entry, uint64_t base, uint32_t limit, uint8_t access, uint8_t gran) {
        SetEntry((GDTEntry*)entry, base, limit, access, gran);
        entry->base_upper = (base >> 32) & 0xFFFFFFFF;
        entry->reserved   = 0;
    }

    void SetTSSStack(uint64_t kernel_stack) {
        tss.rsp0 = kernel_stack;
    }
    
    void initialize() {
        Debug::krnl_print("GDT", Debug::LOG_INFO, "Initialize");

        memset(&gdt_table, 0, sizeof(GDT));
        memset(&tss, 0, sizeof(TSS));

        uint64_t stack_ptr;
        asm volatile("mov %%rsp, %0" : "=r"(stack_ptr));
        tss.rsp0 = stack_ptr;

        tss.ist[TSS_IST_DOUBLE_FAULT] = (uint64_t)double_fault_stack + sizeof(double_fault_stack);
        tss.ist[TSS_IST_TIMER] = (uint64_t)timer_stack + sizeof(timer_stack);
        tss.ist[TSS_IST_NMI] = (uint64_t)nmi_stack + sizeof(nmi_stack);

        SetEntry(&gdt_table.null, 0, 0, 0, 0);

        SetEntry(&gdt_table.kernel_code, 0, 0xFFFFF, Access::KERNEL_CODE, Granularity::PAGE_4K | Granularity::MODE_64_BIT);
        SetEntry(&gdt_table.kernel_data, 0, 0xFFFFF, Access::KERNEL_DATA, Granularity::PAGE_4K);

        SetEntry(&gdt_table.user_code, 0, 0xFFFFF, Access::USER_CODE, Granularity::PAGE_4K | Granularity::MODE_64_BIT);
        SetEntry(&gdt_table.user_data, 0, 0xFFFFF, Access::USER_DATA, Granularity::PAGE_4K);

        SetTSSEntry(&gdt_table.tss, (uint64_t)&tss, sizeof(TSS) - 1, 0x89, 0);

        GDTDescriptor gdtr;
        gdtr.size = sizeof(GDT) - 1;
        gdtr.offset = (uint64_t)&gdt_table;

        LoadGDT(&gdtr);

        asm volatile("ltr %%ax" : : "a"(0x28));

        return;
    }
}