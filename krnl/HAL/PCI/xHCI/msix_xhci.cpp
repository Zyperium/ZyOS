#include <HAL/PCI/xHCI/xHCI.hpp>
#include <HAL/PCI/xHCI/msix_xhci.hpp>
#include <HAL/CORE/Core.hpp>
#include <Library/regs.h>
#include <Scheduler/Scheduler.hpp>
namespace HAL::PCI {
    namespace MSIX::xHCI {
        using ::HAL::PCI::xHCI;
        xHCI *xHCI_instances[MAX_XHCI_INSTANCES]{nullptr};
        size_t curr_count{};

        struct bulk_queue {
            xHCI *instance;
            uint8_t slot_id;
            uint8_t endpoint_addr;
            uint64_t buff_phys;
            uint32_t buffer_size;
            volatile bulk_queue *next;
        };

        volatile bulk_queue *wrapper = nullptr;
        bool do_a_log = false;

        Scheduler::Task *xHCI_worker;
        bool a_msix_lock = false;
        uint64_t cur_rflags = 0;
        uint64_t core_id_holder = -1;
        int nested = 0;
        
        void aquire_lock() {
            return;
            if (core_id_holder == HAL::CORE::get_thread_data()->current_task->get_pid()) {
                ++nested;
                return;
            }

            uint64_t rflags = 0;
            asm volatile("pushfq; pop %0" : "=r"(rflags));
            while (__atomic_test_and_set(&a_msix_lock, __ATOMIC_ACQUIRE)) {
                asm volatile("pause");
            }

            // asm volatile("cli");

            cur_rflags = rflags;
            core_id_holder = HAL::CORE::get_thread_data()->current_task->get_pid();

            return;
        }

        void release_lock() {
            return;
            if (nested > 0) {
                nested--;
                return;
            }

            restore_rflags(cur_rflags);
            cur_rflags = 0;
            core_id_holder = -1;
            __atomic_clear(&a_msix_lock, __ATOMIC_RELEASE);
            return;
        }

        int current_loops{};
        void worker() {
            for (;;) {
                if (do_a_log) {
                    // Debug::krnl_print("MSIX", Debug::LOG_INFO, "Worker CR3: %x, Wrapper Addr: %x", read_cr3(), &wrapper);
                }

                while (wrapper) {
                    Debug::krnl_print("MSIX", Debug::LOG_INFO, "Invoking bulk transfer!");
                    aquire_lock();
                    wrapper->instance->queue_bulk_transfer(wrapper->slot_id, 
                        wrapper->endpoint_addr,
                        wrapper->buff_phys,
                        wrapper->buffer_size  
                    );

                    volatile bulk_queue *old_wrapper = wrapper;

                    wrapper = wrapper->next;
                    delete old_wrapper;
                    release_lock();
                }

                bool did_work = false;
                for (auto i{0uz}; i < MAX_XHCI_INSTANCES; ++i) {
                    if (!xHCI_instances[i]) continue;
                    aquire_lock();
                    asm volatile("sti"); // important! (Otherwise the OS will deadlock) [also rflags will be reset to whatever they were after release_lock]
                    did_work = did_work || xHCI_instances[i]->poll_event_ring();
                    release_lock();
                }

                if (!did_work)
                    current_loops++;
                else
                    MSIX::xHCI::current_loops = 0;
                // if (current_loops >= LOOPS_BEFORE_YIELD)
                //     xHCI_worker->block(Scheduler::BlockReasons::AWAIT_MSIX_EVENT);
                // Debug::krnl_print("MSIX", Debug::LOG_INFO, "current_loops %i", current_loops);

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
            return;
        }

        void queue_bulk_transfer(xHCI *inst, uint8_t slot_id, uint8_t endpoint_address, uint64_t buffer_phys, uint32_t buffer_size) {
            aquire_lock();
            bulk_queue *blk = new bulk_queue {
                inst,
                slot_id,
                endpoint_address,
                buffer_phys,
                buffer_size,
                nullptr
            };

            Debug::krnl_print("MSIX", Debug::LOG_INFO, "Queuing new bulk transfer for later handling");

            if (!wrapper) {
                wrapper = blk;
                // Debug::krnl_print("MSIX", Debug::LOG_INFO, "Shell CR3: %x, Wrapper Addr: %x", read_cr3(), &wrapper);
                do_a_log = true;
                release_lock();
                asm volatile("sti");
                Scheduler::Yield();
                return;
            }
            
            blk->next = wrapper;
            wrapper = blk;
            // Debug::krnl_print("MSIX", Debug::LOG_INFO, "Shell CR3: %x, Wrapper Addr: %x", read_cr3(), &wrapper);
            do_a_log = true;
            release_lock();
            asm volatile("sti");
            Scheduler::Yield();
        }
    }
}

using namespace HAL::PCI;

extern "C" void xHCIIntHandler() {
    MSIX::xHCI::current_loops = 0;
    MSIX::xHCI::xHCI_worker->unblock(Scheduler::BlockReasons::AWAIT_MSIX_EVENT);
    
    return;
}
