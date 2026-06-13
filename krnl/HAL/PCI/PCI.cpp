#include <HAL/PCI/PCI.hpp>
#include <HAL/MEM/PMEM.hpp>
#include <HAL/PCI/xHCI/msix_xhci.hpp>
#include <HAL/PCI/xHCI/xHCI.hpp>
#include <HAL/CORE/Core.hpp>

#include <Library/io.hpp>
#include <Library/debug.hpp>
#include <Library/regs.h>

namespace HAL::PCI {
    bool a_msix_lock = false;
    uint64_t pci_cur_rflags = 0;
        
    void pci_aquire_lock() {
        return;
        uint64_t rflags = 0;
        asm volatile("pushfq; pop %0" : "=r"(rflags));
        while (__atomic_test_and_set(&a_msix_lock, __ATOMIC_ACQUIRE)) {
            asm volatile("pause");
        }

        asm volatile("cli");
        pci_cur_rflags = rflags;
        return;
    }

    void pci_release_lock() {
        return;
        restore_rflags(pci_cur_rflags);

        pci_cur_rflags = 0;

        __atomic_clear(&a_msix_lock, __ATOMIC_RELEASE);
        return;
    }

    uint32_t Read32(uint8_t bus, uint8_t device, uint8_t func, uint8_t offset) {
        pci_aquire_lock();
        uint32_t address = PCI_ENABLE_BIT |
                       ((uint32_t)bus << PCI_BUS_SHIFT) |
                       ((uint32_t)device << PCI_DEVICE_SHIFT) |
                       ((uint32_t)func << PCI_FUNC_SHIFT) |
                       (offset & PCI_OFFSET_ALIGN);

        outl(CONF_ADDR, address);
        pci_release_lock();
        return inl(CONF_DATA);
    }

    uint16_t Read16(uint8_t bus, uint8_t device, uint8_t func, uint8_t offset) {
        uint32_t val = Read32(bus, device, func, offset);
        uint32_t shift_amount = (offset & PCI_16BIT_ALIGN_MASK) * PCI_BYTE_SHIFT_MULTIPLIER;
        return static_cast<uint16_t>((val >> shift_amount) & PCI_16BIT_MASK);
    }

    uint8_t Read8(uint8_t bus, uint8_t device, uint8_t func, uint8_t offset) {
        uint32_t val = Read32(bus, device, func, offset);
        uint32_t shift_amount = (offset & PCI_8BIT_ALIGN_MASK) * PCI_BYTE_SHIFT_MULTIPLIER;
        return static_cast<uint8_t>((val >> shift_amount) & PCI_8BIT_MASK);
    }

    void Write32(uint8_t bus, uint8_t device, uint8_t func, uint8_t offset, uint32_t value) {
        pci_aquire_lock();
        uint32_t address = PCI_ENABLE_BIT |
                       ((uint32_t)bus << PCI_BUS_SHIFT) |
                       ((uint32_t)device << PCI_DEVICE_SHIFT) |
                       ((uint32_t)func << PCI_FUNC_SHIFT) |
                       (offset & PCI_OFFSET_ALIGN);

        outl(CONF_ADDR, address);
        outl(CONF_DATA, value);
        pci_release_lock();
        return;
    }

    void Write16(uint8_t bus, uint8_t device, uint8_t func, uint8_t offset, uint16_t value) {
        uint32_t current = Read32(bus, device, func, offset);
        uint32_t shift = (offset & PCI_16BIT_ALIGN_MASK) * PCI_BYTE_SHIFT_MULTIPLIER;
        
        current &= ~(PCI_16BIT_MASK << shift);
        current |= ((uint32_t)value << shift);
        
        Write32(bus, device, func, offset, current);
    }

    uint64_t GetBAR(uint8_t bus, uint8_t device, uint8_t function, uint8_t bar_index) {
        uint8_t offset = PCI_BAR_START_OFFSET + (bar_index * PCI_BAR_SIZE);

        uint32_t bar_low = Read32(bus, device, function, offset);

        uint8_t type = (bar_low >> PCI_BAR_TYPE_SHIFT) & PCI_BAR_TYPE_MASK;

        uint64_t final_addr = bar_low & PCI_BAR_MEM_ADDR_MASK;

        if (type == PCI_BAR_TYPE_64BIT) {
            uint32_t bar_high = Read32(bus, device, function, offset + PCI_BAR_SIZE);
            final_addr |= ((uint64_t)bar_high << 32);
        }

        return final_addr;
    }

    void EnableBusMaster(uint8_t bus, uint8_t device, uint8_t function) {
        uint32_t cmd = Read32(bus, device, function, PCI_COMMAND_REG_OFFSET);

        cmd |= (PCI_CMD_MEMORY_SPACE_EN | PCI_CMD_BUS_MASTER_EN);

        Write32(bus, device, function, PCI_COMMAND_REG_OFFSET, cmd);
    }

    void CheckDevice(uint8_t bus, uint8_t device) {
        constexpr uint8_t primary_func = 0;
        uint32_t id_reg = Read32(bus, device, primary_func, PCI_REG_IDENTIFICATION);
        uint16_t vendor = id_reg & PCI_16BIT_MASK;
        if (vendor == PCI_INVALID_VENDOR_ID) return;

        uint32_t header_reg = Read32(bus, device, primary_func, PCI_REG_BIST_HEADER);
        uint8_t header_type = (header_reg >> PCI_HEADER_TYPE_SHIFT) & PCI_HEADER_TYPE_MASK;

        uint8_t function_count = (header_type & PCI_HEADER_MULTI_FUNC) ? PCI_MAX_FUNCTIONS : 1;

        for (uint8_t f = 0; f < function_count; f++) {
            uint32_t val0 = Read32(bus, device, f, PCI_REG_IDENTIFICATION);
            uint16_t v_id = val0 & PCI_16BIT_MASK;
            uint16_t d_id = (val0 >> PCI_BUS_SHIFT) & PCI_16BIT_MASK;

            if (v_id == PCI_INVALID_VENDOR_ID) continue;

            uint32_t val8 = Read32(bus, device, f, PCI_REG_CLASS_REVISION);
            uint8_t class_code = (val8 >> 24) & PCI_8BIT_MASK;
            uint8_t subclass = (val8 >> 16) & PCI_8BIT_MASK;
            uint8_t prog_if = (val8 >> 8)  & PCI_8BIT_MASK;

            Debug::krnl_print("PCI", Debug::LOG_INFO, "Found new device. Data:");
            Debug::krnl_print("PCI", Debug::LOG_INFO, "Address: %i:%i:%i", bus, device, f);
            Debug::krnl_print("PCI", Debug::LOG_INFO, "Vendor: %x, Device: %x", v_id, d_id);
            Debug::krnl_print("PCI", Debug::LOG_INFO, "Class: %x, Sub: %x, ProgIF: %x", class_code, subclass, prog_if);

            if (class_code == PCI_CLASS_SERIAL_BUS && subclass == PCI_SUBCLASS_USB) {
                if (prog_if == PCI_PROGIF_USB_UHCI) 
                    Debug::krnl_print("PCI", Debug::LOG_INFO, "Type of: USB UHCI");
                else if (prog_if == PCI_PROGIF_USB_OHCI) 
                    Debug::krnl_print("PCI", Debug::LOG_INFO, "Type of: USB OHCI");
                else if (prog_if == PCI_PROGIF_USB_EHCI) 
                    Debug::krnl_print("PCI", Debug::LOG_INFO, "Type of: USB EHCI");
                else if (prog_if == PCI_PROGIF_USB_XHCI) {
                    Debug::krnl_print("PCI", Debug::LOG_INFO, "Type of: USB xHCI");
                    if (MSIX::xHCI::curr_count < MSIX::xHCI::MAX_XHCI_INSTANCES - 1) {
                        EnableBusMaster(bus, device, f);
                        xHCI *driver = new xHCI(bus, device, f);
                        driver->initialize();
                    }
                }
            }

            if (class_code == PCI_CLASS_MASS_STORAGE) {
                if (subclass == PCI_SUBCLASS_IDE) {
                    Debug::krnl_print("PCI", Debug::LOG_INFO, "Type of: IDE CONTROLLER");
                }
                else if (subclass == PCI_SUBCLASS_SATA) {
                    Debug::krnl_print("PCI", Debug::LOG_INFO, "Type of: SATA AHCI Controller");
                }
            }
        }
    }

    void EnumeratePCI() {
        Debug::krnl_print("PCI", Debug::LOG_INFO, "Starting enumeration...");
        for (uint16_t bus = 0; bus < MAX_PCI_BUS; bus++) {
            for (uint8_t dev = 0; dev < MAX_PCI_DEV; dev++) {
                CheckDevice(bus, dev);
            }
        }
        
        Debug::krnl_print("PCI", Debug::LOG_INFO, "Enumeration complete");
    }

    void EnableMSIX(uint8_t bus, uint8_t device, uint8_t func, uint8_t vector, uint8_t apic_id) {
        uint16_t status = Read16(bus, device, func, PCI_REG_STATUS);
        if (!(status & PCI_STATUS_CAP_LIST_BIT)) return;

        uint8_t cap_ptr = Read8(bus, device, func, PCI_REG_CAP_POINTER) & PCI_CAP_PTR_ALIGN_MASK;

        while (cap_ptr) {
            uint8_t cap_id = Read8(bus, device, func, cap_ptr);
            if (cap_id == PCI_CAP_ID_MSIX) {
                Debug::krnl_print("PCI", Debug::LOG_INFO, "Enabling MSI-X");
                uint16_t msg_ctrl = Read16(bus, device, func, cap_ptr + PCI_MSIX_REG_MSG_CTRL);
                uint32_t table_offset_bir = Read32(bus, device, func, cap_ptr + PCI_MSIX_REG_TABLE_OFF);
                
                uint8_t bir = table_offset_bir & PCI_MSIX_BIR_MASK;
                uint32_t table_offset = table_offset_bir & ~PCI_MSIX_BIR_MASK;
                uint64_t bar_phys = GetBAR(bus, device, func, bir);
                volatile MSIXTableEntry *msix_table = (volatile MSIXTableEntry*)MEM::PMEM::map_mmio(
                    bar_phys + table_offset, 1
                );

                msix_table[0].msg_addr_low = LAPIC_BASE_MSI_ADDR | (apic_id << LAPIC_SHIFT_DEST_ID);
                msix_table[0].msg_addr_high = 0;
                msix_table[0].msg_data = vector; 
                msix_table[0].vector_ctrl &= ~PCI_MSIX_VECTOR_UNMASK_BIT; 

                msg_ctrl |= PCI_MSIX_CTRL_ENABLE_BIT;
                Write16(bus, device, func, cap_ptr + PCI_MSIX_REG_MSG_CTRL, msg_ctrl);
                return; 
            }

            if (cap_id == PCI_CAP_ID_MSI) {
                Debug::krnl_print("PCI", Debug::LOG_INFO, "Disabling MSI");
                uint16_t msi_msg_ctrl = Read16(bus, device, func, cap_ptr + 2);
                msi_msg_ctrl &= ~1;
                Write16(bus, device, func, cap_ptr + 2, msi_msg_ctrl);
            }
            
            cap_ptr = Read8(bus, device, func, cap_ptr + 1) & PCI_CAP_PTR_ALIGN_MASK;
        }
    }
}