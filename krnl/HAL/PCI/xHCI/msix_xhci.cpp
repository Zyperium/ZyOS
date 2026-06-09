#include <HAL/PCI/xHCI/xHCI.hpp>
#include <HAL/PCI/xHCI/msix_xhci.hpp>
#include <Scheduler/Scheduler.hpp>

namespace HAL::PCI {
    namespace MSIX::xHCI {
        using ::HAL::PCI::xHCI;
        xHCI *xHCI_instances[MAX_XHCI_INSTANCES];
        size_t curr_count = 0;

        Scheduler::Task *xHCI_worker;

        void worker() {
            xHCI_worker->block(Scheduler::BlockReasons::AWAIT_MSIX_EVENT);
        }

        void register_xhci_worker(xHCI *class_instance) {
            if (curr_count >= MAX_XHCI_INSTANCES) return;
            xHCI_instances[curr_count++] = class_instance;
        }
    }
}

using namespace HAL::PCI;

extern "C" void xHCIIntHandler() {
    // basically just tell the scheduler to unblock the worker.
    MSIX::xHCI::xHCI_worker->unblock(Scheduler::BlockReasons::AWAIT_MSIX_EVENT);
    return;
}
