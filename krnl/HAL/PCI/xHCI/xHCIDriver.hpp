#pragma once
#include <stdint.h>
#include <Library/debug.hpp>

namespace HAL::PCI {
    class xHCI;
    class xHCIDriver {
    public:
        virtual ~xHCIDriver() = default;
        virtual void initialize(xHCI* _ctrl, uint8_t _slot, void *endpoints, int ep_count) = 0;
        virtual void on_int(uint32_t bytes_transferred, uint32_t endpoint_id, uint64_t param_event) = 0;
        virtual void start() = 0;
    };

    constexpr uint8_t CLASS_USB_HUB = 0x09;
    constexpr uint8_t CLASS_HID = 0x03;
    constexpr uint8_t CLASS_MASS_STORAGE = 0x08;

    constexpr uint8_t BOOT_IFACE_SUBCLASS = 0x1;

    constexpr uint8_t PROT_HID_KEYBOARD = 0x01;
    constexpr uint8_t PROT_HID_MOUSE = 0x02;

    xHCIDriver *LoadNewDriver(uint8_t classCode, uint8_t subClass, uint8_t protocol);
}