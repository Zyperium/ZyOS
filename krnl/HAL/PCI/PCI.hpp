#pragma once
#include <stdint.h>

namespace HAL::PCI {
    struct PCIDevice {
        uint8_t bus;
        uint8_t device;
        uint8_t function;
        uint16_t vendor_id;
        uint16_t device_id;
        uint8_t class_id;
        uint8_t subclass_id;
        uint8_t prog_if;
    };

    struct MSIXTableEntry {
        uint32_t msg_addr_low;
        uint32_t msg_addr_high;
        uint32_t msg_data;
        uint32_t vector_ctrl;
    };

    void EnumeratePCI();
    uint32_t Read32(uint8_t bus, uint8_t device, uint8_t func, uint8_t offset);
    uint64_t GetBAR(uint8_t bus, uint8_t device, uint8_t function, uint8_t bar_index);
    void EnableBusMaster(uint8_t bus, uint8_t device, uint8_t function);
    void EnableMSIX(uint8_t bus, uint8_t device, uint8_t func, uint8_t vector, uint8_t apic_id);

    constexpr uint16_t CONF_ADDR = 0xCF8;
    constexpr uint16_t CONF_DATA = 0xCFC;
    constexpr uint32_t PCI_ENABLE_BIT = 1U << 31;
    constexpr uint32_t PCI_BUS_SHIFT = 16;
    constexpr uint32_t PCI_DEVICE_SHIFT = 11;
    constexpr uint32_t PCI_FUNC_SHIFT = 8;
    constexpr uint32_t PCI_OFFSET_ALIGN = 0xFC;

    constexpr uint32_t PCI_BYTE_SHIFT_MULTIPLIER = 8;
    constexpr uint32_t PCI_16BIT_ALIGN_MASK = 0x02;
    constexpr uint32_t PCI_8BIT_ALIGN_MASK = 0x03;
    constexpr uint16_t PCI_16BIT_MASK = 0xFFFF;
    constexpr uint8_t PCI_8BIT_MASK = 0xFF;

    constexpr uint8_t PCI_BAR_START_OFFSET = 0x10;
    constexpr uint8_t PCI_BAR_SIZE = 4;

    constexpr uint32_t PCI_BAR_TYPE_MASK  = 0x3;
    constexpr uint32_t PCI_BAR_TYPE_SHIFT  = 1;
    constexpr uint32_t PCI_BAR_TYPE_64BIT  = 0x2;

    constexpr uint32_t PCI_BAR_MEM_ADDR_MASK  = 0xFFFFFFF0;
    constexpr uint32_t PCI_BAR_IO_ADDR_MASK  = 0xFFFFFFFC; 
    constexpr uint32_t PCI_BAR_IO_BIT  = 0x1;

    constexpr uint8_t PCI_COMMAND_REG_OFFSET = 0x04;
    constexpr uint32_t PCI_CMD_MEMORY_SPACE_EN = 0x02;
    constexpr uint32_t PCI_CMD_BUS_MASTER_EN = 0x04;

    constexpr uint8_t PCI_REG_IDENTIFICATION = 0x00;
    constexpr uint8_t PCI_REG_CLASS_REVISION = 0x08;
    constexpr uint8_t PCI_REG_BIST_HEADER = 0x0C;

    constexpr uint32_t PCI_HEADER_TYPE_SHIFT = 16;
    constexpr uint32_t PCI_HEADER_TYPE_MASK = 0xFF;
    constexpr uint8_t PCI_HEADER_MULTI_FUNC = 0x80;

    constexpr uint8_t PCI_MAX_FUNCTIONS = 8;
    constexpr uint16_t PCI_INVALID_VENDOR_ID = 0x810;

    constexpr uint8_t PCI_CLASS_MASS_STORAGE = 0x01;
    constexpr uint8_t PCI_CLASS_SERIAL_BUS = 0x0C;

    constexpr uint8_t PCI_SUBCLASS_IDE = 0x01;
    constexpr uint8_t PCI_SUBCLASS_SATA = 0x06;
    constexpr uint8_t PCI_SUBCLASS_USB = 0x03;

    constexpr uint8_t PCI_PROGIF_USB_UHCI = 0x00;
    constexpr uint8_t PCI_PROGIF_USB_OHCI = 0x10;
    constexpr uint8_t PCI_PROGIF_USB_EHCI = 0x20;
    constexpr uint8_t PCI_PROGIF_USB_XHCI = 0x30;

    constexpr uint8_t PCI_BAR_5 = 5;
    constexpr uint16_t MAX_PCI_BUS = 256;
    constexpr uint8_t MAX_PCI_DEV = 32;

    constexpr uint8_t PCI_REG_STATUS = 0x06;
    constexpr uint8_t PCI_REG_CAP_POINTER = 0x34;

    constexpr uint16_t PCI_STATUS_CAP_LIST_BIT = 1 << 4;
    constexpr uint8_t PCI_CAP_PTR_ALIGN_MASK = 0xFC;

    constexpr uint8_t PCI_CAP_ID_MSIX = 0x11;

    constexpr uint8_t PCI_MSIX_REG_MSG_CTRL = 2;
    constexpr uint8_t PCI_MSIX_REG_TABLE_OFF = 4;

    constexpr uint16_t PCI_MSIX_CTRL_ENABLE_BIT = 1 << 15;
    constexpr uint32_t PCI_MSIX_BIR_MASK = 0x7;

    constexpr uint32_t LAPIC_BASE_MSI_ADDR = 0xFEE00000;
    constexpr uint32_t LAPIC_SHIFT_DEST_ID = 12;
    constexpr uint32_t PCI_MSIX_VECTOR_UNMASK_BIT = ~1;
}