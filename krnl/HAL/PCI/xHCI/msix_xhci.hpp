#pragma once
#include <stdint.h>
#include <stddef.h>

namespace HAL::PCI {
    class xHCI;
    namespace MSIX::xHCI {
        constexpr uint8_t MAX_XHCI_INSTANCES = 8;
        extern size_t curr_count;
        extern ::HAL::PCI::xHCI *xHCI_instances[];
        void worker();
        void register_xhci_worker(::HAL::PCI::xHCI *class_instance);
        void create_xhci_worker();
        void queue_bulk_transfer(::HAL::PCI::xHCI *inst, uint8_t slot_id, uint8_t endpoint_address, uint64_t buffer_phys, uint32_t buffer_size);

        constexpr int LOOPS_BEFORE_YIELD = 300;
    }
}
