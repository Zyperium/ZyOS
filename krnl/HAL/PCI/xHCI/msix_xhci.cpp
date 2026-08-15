#include <HAL/MEM/VMM.hpp>
#include <HAL/MEM/PMM.hpp>
#include <HAL/MEM/KMEM.hpp>
#include <Library/debug.hpp>
#include <Library/locks.hpp>
#include <HAL/PCI/xHCI/xHCI.hpp>
#include <HAL/PCI/xHCI/msix_xhci.hpp>
#include <HAL/MEM/PMEM.hpp>
#include <HAL/CORE/Core.hpp>
#include <Library/regs.h>
#include <Services/Scheduler/Scheduler.hpp>
#include <Library/string.h>

namespace HAL::PCI {
    namespace MSIX::xHCI {
        using ::HAL::PCI::xHCI;

        xHCI *xHCI_instances[MAX_XHCI_INSTANCES]{nullptr};
        size_t curr_count{};

        lib::Spinlock msixlock;

        struct bulk_request {
            xHCI *instance;
            uint64_t buff_phys;
            uint32_t buffer_size;
            uint8_t slot_id;
            uint8_t endpoint_addr;
        };

        constexpr size_t QUEUE_CAPACITY = ((PAGE_SIZE * 2) / sizeof(bulk_request)) - 1; 

        struct bulk_ring {
            bulk_request requests[QUEUE_CAPACITY];
            size_t head{0};
            size_t tail{0};
            size_t count{0};
        };

        static bulk_ring *ring = nullptr;

        Scheduler::Task *xHCI_worker;
        int current_loops{};

        void worker() {
            if (!ring) {
                auto p = HAL::MEM::PMEM::alloc_pages(2, MEM::VMM::PTE_PRESENT | MEM::VMM::PTE_WRITABLE | MEM::VMM::PTE_NX);
                ring = (bulk_ring *)(p);
                memset(ring, 0, sizeof(bulk_ring));
            }

            for (;;) {
                while (true) {
                    bulk_request req{};
                    bool has_work = false;

                    {
                        lib::ScopedLock l(msixlock);
                        if (ring && ring->count > 0) {
                            req = ring->requests[ring->tail];
                            ring->tail = (ring->tail + 1) % QUEUE_CAPACITY;
                            ring->count--;
                            has_work = true;
                        }
                    }

                    if (!has_work) break;

                    req.instance->queue_bulk_transfer(
                        req.slot_id,
                        req.endpoint_addr,
                        req.buff_phys,
                        req.buffer_size
                    );
                }

                bool did_work = false;
                for (auto i{0uz}; i < MAX_XHCI_INSTANCES; ++i) {
                    if (!xHCI_instances[i]) continue;
                    did_work = did_work || xHCI_instances[i]->poll_event_ring();
                }

                if (!did_work)
                    current_loops++;
                else
                    MSIX::xHCI::current_loops = 0;

                if (current_loops >= LOOPS_BEFORE_YIELD)
                    xHCI_worker->block(Scheduler::BlockReasons::AWAIT_MSIX_EVENT);

                Scheduler::Yield();
            }
        }

        void register_xhci_worker(xHCI *class_instance) {
            if (curr_count >= MAX_XHCI_INSTANCES) return;
            xHCI_instances[curr_count++] = class_instance;
        }

        void create_xhci_worker() {
            xHCI_worker = new Scheduler::Task((Scheduler::Task::EntryPoint)worker, "xHCI Runner");
            xHCI_worker->core_pinned = true;
            xHCI_worker->current_core = 0;
        }

        void queue_bulk_transfer(xHCI *inst, uint8_t slot_id, uint8_t endpoint_address, uint64_t buffer_phys, uint32_t buffer_size) {
            lib::ScopedLock l(msixlock);

            if (!ring) {
                Debug::krnl_print("MSIX", Debug::LOG_WARN, "Unable to queue, ring not ready!");
                return;
            }

            if (ring->count >= QUEUE_CAPACITY) {
                Debug::krnl_print("MSIX", Debug::LOG_WARN, "xHCI Bulk Queue full! Dropping request.");
                return;
            }

            ring->requests[ring->head] = bulk_request {
                inst,
                buffer_phys,
                buffer_size,
                slot_id,
                endpoint_address
            };

            ring->head = (ring->head + 1) % QUEUE_CAPACITY;
            ring->count++;

            xHCI_worker->unblock(Scheduler::BlockReasons::AWAIT_MSIX_EVENT);
            current_loops = 0;
        }
    }
}

using namespace HAL::PCI;

extern "C" void xHCIIntHandler() {
    MSIX::xHCI::current_loops = 0;
    MSIX::xHCI::xHCI_worker->unblock(Scheduler::BlockReasons::AWAIT_MSIX_EVENT);
    HAL::CORE::ack_lapic();
}