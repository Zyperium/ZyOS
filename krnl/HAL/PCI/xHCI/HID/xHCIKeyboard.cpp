#include <HAL/PCI/xHCI/HID/xHCIKeyboard.hpp>
#include <HAL/PCI/xHCI/HID/xHCIKeymap.hpp>
#include <HAL/PCI/xHCI/xHCI.hpp>
#include <HAL/PCI/xHCI/msix_xhci.hpp>
#include <HAL/MEM/PMEM.hpp>
#include <HAL/MEM/VMM.hpp>
#include <Library/debug.hpp>
#include <Library/string.h>
#include <Library/regs.h>

#include <Services/TTY/TTY.hpp>

using namespace HAL::MEM;

namespace HAL::PCI::HID {
    void USBKeyboard::initialize(PCI::xHCI* _ctrl, uint8_t _slot, void *endpoints_ptr, int ep_count) {
        controller = _ctrl;
        slot_id = _slot;

        Debug::krnl_print("xHCI", Debug::LOG_INFO, "Initializing HID USB Keyboard Driver");

        auto* eps = reinterpret_cast<xHCI::ParsedEndpoint *>(endpoints_ptr);

        for (int i = 0; i < ep_count; i++) {
            uint8_t ep_addr = eps[i].address;
            bool is_in = (ep_addr & EP_DIR_MASK) != 0;
            uint8_t ep_type = eps[i].attributes & EP_ATTR_TYPE_MASK;

            if (ep_type == EP_ATTR_INTERRUPT && is_in) {
                interrupt_in_ep = ep_addr;
                break; 
            }
        }

        if (!interrupt_in_ep) {
            Debug::krnl_print("xHCI", Debug::LOG_WARN, "USB Keyboard missing vital Interrupt IN endpoint!");
            return;
        }

        report_virt = static_cast<KeyboardBootReport *>(PMEM::alloc_page(VMM::PTE_CACHELESS | VMM::PTE_PRESENT | VMM::PTE_WRITABLE));
        report_phys = VMM::GetPhysicalAddress(read_cr3(), reinterpret_cast<uint64_t>(report_virt));

        memset(report_virt, 0, BOOT_REPORT_SIZE);
        memset(&last_report, 0, sizeof(KeyboardBootReport));

        initialized = true;
    }

    void USBKeyboard::start() {
        if (!initialized) return;

        Debug::krnl_print("xHCI", Debug::LOG_INFO, "Starting HID USB Keyboard Polling Loop");

        PCI::MSIX::xHCI::queue_bulk_transfer(controller, slot_id, interrupt_in_ep, report_phys, BOOT_REPORT_SIZE);
    }

    size_t looped = 0;
    void USBKeyboard::on_int(uint32_t bytes_transferred, uint32_t endpoint_id, uint64_t param_event) {
        (void)param_event;

        if (endpoint_id != interrupt_in_ep) {
            Debug::krnl_print("KB", Debug::LOG_INFO, "Received an interrupt, but endpoint id didn't match!");
            return;
        }

        if (bytes_transferred <= BOOT_REPORT_SIZE) {
            KeyboardBootReport current_report = *report_virt;
            process_report(current_report);
        }
        
        PCI::MSIX::xHCI::queue_bulk_transfer(controller, slot_id, interrupt_in_ep, report_phys, BOOT_REPORT_SIZE);
    }

    void USBKeyboard::process_report(const KeyboardBootReport& report) {
        for (size_t i = 0; i < 6; i++) {
            uint8_t code = report.keycodes[i];
            if (code == 0) continue;

            bool is_new_press = true;
            for (size_t j = 0; j < 6; j++) {
                if (last_report.keycodes[j] == code) {
                    is_new_press = false;
                    break;
                }
            }

            if (is_new_press) {
                char full_convert = USBKeymap::Get(code).normal;
                if (report.modifiers & ModifierMask::MOD_LEFT_SHIFT) {
                    full_convert = USBKeymap::Get(code).shifted;
                }

                if (TTY::conhosts[TTY::active_host]) {
                    TTY::conhosts[TTY::active_host]->send_input(full_convert);
                }
            }
        }

        last_report = report;
    }
}