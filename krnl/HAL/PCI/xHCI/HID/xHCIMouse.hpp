#pragma once

#include <stdint.h>
#include <stddef.h>
#include <HAL/PCI/xHCI/xHCIDriver.hpp>

namespace HAL::PCI {
    class xHCI;
}

namespace HAL::PCI::HID {

    struct MouseBootReport {
        uint8_t buttons;
        int8_t x;
        int8_t y;
        int8_t wheel;
    } __attribute__((packed));

    enum MouseButtonMask {
        MOUSE_LEFT   = (1 << 0),
        MOUSE_RIGHT  = (1 << 1),
        MOUSE_MIDDLE = (1 << 2)
    };

    class USBMouse : public PCI::xHCIDriver {
    public:
        static constexpr size_t BOOT_REPORT_SIZE = sizeof(MouseBootReport);

        virtual void initialize(PCI::xHCI *_ctrl, uint8_t _slot, void *endpoints, int ep_count) override;
        virtual void on_int(uint32_t bytes_transferred, uint32_t endpoint_id, uint64_t param_event) override;
        virtual void start() override;

    private:
        void process_report(const MouseBootReport &report);

        PCI::xHCI *controller = nullptr;
        uint8_t slot_id = 0;
        uint8_t interrupt_in_ep = 0;
        bool initialized = false;

        MouseBootReport *report_virt = nullptr;
        uint64_t report_phys = 0;
        MouseBootReport last_report{};
    };

}