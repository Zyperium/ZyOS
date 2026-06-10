#pragma once
#include <stdint.h>
#include <HAL/PCI/xHCI/xHCIDriver.hpp>

namespace HAL::PCI {
    namespace HID {
        enum ModifierMask : uint8_t {
            MOD_LEFT_CTRL   = (1 << 0),
            MOD_LEFT_SHIFT  = (1 << 1),
            MOD_LEFT_ALT    = (1 << 2),
            MOD_LEFT_GUI    = (1 << 3),
            MOD_RIGHT_CTRL  = (1 << 4),
            MOD_RIGHT_SHIFT = (1 << 5),
            MOD_RIGHT_ALT   = (1 << 6),
            MOD_RIGHT_GUI   = (1 << 7)
        };

        struct KeyboardBootReport {
            uint8_t modifiers;
            uint8_t reserved;
            uint8_t keycodes[6];
        } __attribute__((packed));

        constexpr uint8_t EP_DIR_MASK        = 0x80;
        constexpr uint8_t EP_ATTR_TYPE_MASK  = 0x03;
        constexpr uint8_t EP_ATTR_INTERRUPT  = 0x03;
        constexpr size_t  BOOT_REPORT_SIZE   = sizeof(KeyboardBootReport);

        class USBKeyboard : public PCI::xHCIDriver {
        private:
            xHCI* controller{nullptr};
            uint8_t slot_id{0};
            uint8_t interrupt_in_ep{0};

            KeyboardBootReport* report_virt{nullptr};
            uint64_t report_phys{0};

            bool initialized{false};

            KeyboardBootReport last_report{};

            void process_report(const KeyboardBootReport& report);

        public:
            USBKeyboard() = default;
            virtual ~USBKeyboard() override = default;

            virtual void initialize(PCI::xHCI* _ctrl, uint8_t _slot, void *endpoints, int ep_count) override;
            virtual void on_int(uint32_t bytes_transferred, uint32_t endpoint_id, uint64_t param_event) override;
            virtual void start() override;
        };
    }
}
