#pragma once

namespace HAL::PCI {
    class xHCI;
    namespace MSIX::xHCI {
        void worker();
        void register_xhci_worker(::HAL::PCI::xHCI *class_instance);
    }
}
