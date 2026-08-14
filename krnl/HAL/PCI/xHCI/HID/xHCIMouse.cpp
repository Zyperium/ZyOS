#include <HAL/PCI/xHCI/HID/xHCIMouse.hpp>
#include <HAL/PCI/xHCI/HID/xHCIKeyboard.hpp>
#include <HAL/PCI/xHCI/xHCI.hpp>
#include <HAL/PCI/xHCI/msix_xhci.hpp>
#include <HAL/MEM/PMEM.hpp>
#include <HAL/MEM/VMM.hpp>
#include <Services/Input/Input.hpp>
#include <Library/debug.hpp>
#include <Library/string.h>
#include <Library/regs.h>

using namespace HAL::MEM;

namespace HAL::PCI::HID {

    void USBMouse::initialize(PCI::xHCI *_ctrl, uint8_t _slot, void *endpoints_ptr, int ep_count) {
        controller = _ctrl;
        slot_id = _slot;

        Debug::krnl_print("xHCI", Debug::LOG_INFO, "Initializing HID USB Mouse Driver");

        auto *eps = reinterpret_cast<xHCI::ParsedEndpoint *>(endpoints_ptr);

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
            Debug::krnl_print("xHCI", Debug::LOG_WARN, "USB Mouse missing vital Interrupt IN endpoint!");
            return;
        }

        controller->send_control_request(slot_id, 0x21, 0x0B, 0, 0, 0, 0);

        controller->send_control_request(slot_id, 0x21, 0x0A, 0, 0, 0, 0);

        report_virt = static_cast<MouseBootReport *>(PMEM::alloc_page(VMM::PTE_CACHELESS | VMM::PTE_PRESENT | VMM::PTE_WRITABLE));
        report_phys = VMM::GetPhysicalAddress(read_cr3(), reinterpret_cast<uint64_t>(report_virt));

        memset(report_virt, 0, BOOT_REPORT_SIZE);
        memset(&last_report, 0, sizeof(MouseBootReport));

        initialized = true;
    }

    void USBMouse::start() {
        if (!initialized) return;

        Debug::krnl_print("xHCI", Debug::LOG_INFO, "Starting HID USB Mouse Polling Loop");

        PCI::MSIX::xHCI::queue_bulk_transfer(controller, slot_id, interrupt_in_ep, report_phys, BOOT_REPORT_SIZE);
    }

    void USBMouse::on_int(uint32_t bytes_transferred, uint32_t endpoint_id, uint64_t param_event) {
        (void)param_event;

        if (endpoint_id != interrupt_in_ep) {
            return;
        }

        if (bytes_transferred <= BOOT_REPORT_SIZE) {
            MouseBootReport current_report = *report_virt;
            process_report(current_report);
        }

        PCI::MSIX::xHCI::queue_bulk_transfer(controller, slot_id, interrupt_in_ep, report_phys, BOOT_REPORT_SIZE);
    }

    void USBMouse::process_report(const MouseBootReport &report) {
        int8_t delta_x = report.x;
        int8_t delta_y = report.y;
        uint8_t buttons = report.buttons;
        int8_t scroll_wheel = report.wheel;

        if (delta_x != 0 || delta_y != 0 || scroll_wheel != 0 || buttons != last_report.buttons) {
            Input::add_mouse({delta_x, delta_y, buttons, scroll_wheel});
        }

        last_report = report;
    }

}