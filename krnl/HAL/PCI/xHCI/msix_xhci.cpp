#include <HAL/PCI/xHCI/xHCI.hpp>
#include <HAL/PCI/xHCI/msix_xhci.hpp>
#include <Scheduler/Scheduler.hpp>
namespace HAL::PCI {
    namespace MSIX::xHCI {
        using ::HAL::PCI::xHCI;
        xHCI *xHCI_instances[MAX_XHCI_INSTANCES]{nullptr};
        size_t curr_count{};

        Scheduler::Task *xHCI_worker;

        int current_loops{};
        void worker() {
            loop:
            bool did_work = false;
            for (auto i{0uz}; i < MAX_XHCI_INSTANCES; ++i) {
                if (!xHCI_instances[i]) continue;

                did_work = did_work || xHCI_instances[i]->poll_event_ring();
            }

            if (!did_work)
                current_loops++;
            else
                MSIX::xHCI::current_loops = 0;
            // if (current_loops >= LOOPS_BEFORE_YIELD)
            //     xHCI_worker->block(Scheduler::BlockReasons::AWAIT_MSIX_EVENT);
            goto loop;
        }

        void register_xhci_worker(xHCI *class_instance) {
            if (curr_count >= MAX_XHCI_INSTANCES) return;
            xHCI_instances[curr_count++] = class_instance;
        }

        void create_xhci_worker() {
            xHCI_worker = new Scheduler::Task((Scheduler::Task::EntryPoint)worker, "xHCI Runner");
            return;
        }
    }
}

using namespace HAL::PCI;

extern "C" void xHCIIntHandler() {
    Debug::krnl_print("MSIX", Debug::LOG_INFO, "MSI-X interrupt triggered!");
    MSIX::xHCI::current_loops = 0;
    MSIX::xHCI::xHCI_worker->unblock(Scheduler::BlockReasons::AWAIT_MSIX_EVENT);
    
    return;
}
