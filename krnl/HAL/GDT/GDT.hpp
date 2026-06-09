#pragma once
#include <stdint.h>

namespace HAL::GDT {
    struct TSS {
        uint32_t reserved0;
        uint64_t rsp0;
        uint64_t rsp1;
        uint64_t rsp2;
        uint64_t reserved1;
        uint64_t ist[7];
        uint64_t reserved2;
        uint16_t reserved3;
        uint16_t iomap_base;
    } __attribute__((packed));

    struct GDTEntry {
        uint16_t limit_low;
        uint16_t base_low;
        uint8_t  base_middle;
        uint8_t  access;
        uint8_t  granularity;
        uint8_t  base_high;
    } __attribute__((packed));

    struct GDTEntryTSS {
        uint16_t limit_low;
        uint16_t base_low;
        uint8_t  base_middle;
        uint8_t  access;
        uint8_t  granularity;
        uint8_t  base_high;
        uint32_t base_upper;
        uint32_t reserved;
    } __attribute__((packed));

    struct GDT {
        GDTEntry      null;
        GDTEntry      kernel_code;
        GDTEntry      kernel_data;
        GDTEntry      user_data;
        GDTEntry      user_code;
        GDTEntryTSS   tss;
    } __attribute__((packed));

    struct GDTDescriptor {
        uint16_t size;
        uint64_t offset;
    } __attribute__((packed));

    extern "C" void LoadGDT(GDTDescriptor *ptr);
    extern TSS tss;
    void SetTSSStack(uint64_t kernel_stack);
    void initialize();
    void ReloadSegments();

    constexpr uint8_t TSS_IST_DOUBLE_FAULT = 0;
    constexpr uint8_t TSS_IST_TIMER = 1;
    constexpr uint8_t TSS_IST_NMI = 2;


    namespace Access {
        constexpr uint8_t PRESENT = 1 << 7;
        constexpr uint8_t RING_0 = 0 << 5;
        constexpr uint8_t RING_1 = 1 << 5;
        constexpr uint8_t RING_2 = 2 << 5;
        constexpr uint8_t RING_3 = 3 << 5;
        constexpr uint8_t DESCRIPTOR_TY = 1 << 4;
        constexpr uint8_t EXECUTABLE = 1 << 3;
        constexpr uint8_t DIRECTION_CON = 1 << 2;
        constexpr uint8_t READ_WRITE = 1 << 1;
        constexpr uint8_t ACCESSED = 1 << 0;
        
        constexpr uint8_t KERNEL_CODE   = PRESENT | RING_0 | DESCRIPTOR_TY | EXECUTABLE | READ_WRITE;
        constexpr uint8_t KERNEL_DATA   = PRESENT | RING_0 | DESCRIPTOR_TY | READ_WRITE;
        constexpr uint8_t USER_CODE     = PRESENT | RING_3 | DESCRIPTOR_TY | EXECUTABLE | READ_WRITE;
        constexpr uint8_t USER_DATA     = PRESENT | RING_3 | DESCRIPTOR_TY | READ_WRITE;
    }

    namespace Granularity {
        constexpr uint8_t PAGE_4K       = 1 << 7;
        constexpr uint8_t MODE_32_BIT   = 1 << 6;
        constexpr uint8_t MODE_64_BIT   = 1 << 5;
        constexpr uint8_t AVAILABLE     = 1 << 4;
    }
}