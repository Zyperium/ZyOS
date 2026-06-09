#pragma once
#include <stdint.h>
#include <stddef.h>

namespace HAL::PCI {
    class xHCI;
    namespace MSIX::xHCI {
        constexpr uint8_t MAX_XHCI_INSTANCES = 8;
        extern size_t curr_count;
        void worker();
        void register_xhci_worker(::HAL::PCI::xHCI *class_instance);
    }
}
