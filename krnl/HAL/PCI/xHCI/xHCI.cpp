#include "HAL/CORE/Core.hpp"
#include <HAL/MEM/PMM.hpp>
#include <HAL/PCI/xHCI/xHCI.hpp>
#include <HAL/PCI/xHCI/msix_xhci.hpp>
#include <HAL/PCI/PCI.hpp>
#include <HAL/MEM/PMEM.hpp>
#include <HAL/MEM/VMM.hpp>
#include <HAL/IDT/Panic.hpp>
#include <HAL/IDT/IDT.hpp>
#include <HAL/MEM/KMEM.hpp>
#include <HAL/ACPI/ACPI.hpp>

#include <Scheduler/Scheduler.hpp>

#include <Library/debug.hpp>
#include <Library/regs.h>
#include <Library/string.h>

namespace HAL::PCI {
    using namespace MEM;


    bool a_xhci_lock = false;
    uint64_t cur_rflags = 0;
    uint64_t core_id_holder = -1;
    int reentrancy = 0;

    // I don't know why, but these locks cause it to break.

    void aquire_lock() {
        return;
        if (core_id_holder == HAL::CORE::get_thread_data()->current_task->get_pid()) {
            reentrancy++;
            return;
        }

        uint64_t rflags = 0;
        asm volatile("pushfq; pop %0" : "=r"(rflags));
        while (__atomic_test_and_set(&a_xhci_lock, __ATOMIC_ACQUIRE)) {
            asm volatile("pause");
        }

        cur_rflags = rflags;
        core_id_holder = HAL::CORE::get_thread_data()->current_task->get_pid();

        return;
    }

    void release_lock() {
        return;
        if (reentrancy > 0) {
            reentrancy--;
            return;
        }
        
        restore_rflags(cur_rflags);
        cur_rflags = 0;
        core_id_holder = -1;

        __atomic_clear(&a_xhci_lock, __ATOMIC_RELEASE);
        return;
    }

    xHCI::xHCI(uint8_t bus, uint8_t device, uint8_t function) : pci_bus(bus), pci_device(device),
        pci_func(function), cmd_ring_cycle_state(true), cmd_ring_enqueue_ptr(0), 
        event_ring_dequeue_ptr(0), event_ring_cycle_state(true), pending_port_setup(-1),
        port_slot_mapping(nullptr), ep_rings(nullptr), ep_cycle_states(nullptr), ep_enqueue_ptrs(nullptr),
        descriptor_buffer(nullptr), slot_states(nullptr), config_descriptor_buffer(nullptr) {}

    void xHCI::bios_handoff() {
        const uint32_t hcc_params1 = cap_regs->hcc_params1;
        uint32_t xECP = (hcc_params1 >> HCC_PARAMS_XECP_SHIFT) & HCC_PARAMS_XECP_MASK;

        if (!xECP) return;

        uint32_t cap_offset = xECP * 4;

        while (cap_offset != 0) {
            volatile uint32_t* cap_reg = (volatile uint32_t*)(mmio_base_virt + cap_offset);
            uint32_t val = *cap_reg;
            uint8_t cap_id = val & CAP_ID_MASK;

            if (cap_id == CAP_ID_USB_LEGACY) {
                if (val & USBLEGSUP_BIOS_OWNED) {
                    *cap_reg = val | USBLEGSUP_OS_OWNED;

                    int timeout = 10000; 
                    while ((*cap_reg & USBLEGSUP_BIOS_OWNED) && timeout > 0) {
                        timeout--;
                        asm volatile("pause");
                    }

                    if (*cap_reg & USBLEGSUP_BIOS_OWNED) {
                        Debug::krnl_print("xHCI", Debug::LOG_WARN, "BIOS refused handoff.");
                    }
                }
            }

            uint8_t next = (val >> 8) & 0xFF;
            if (next == 0) break;
            cap_offset += next * 4;
        }
    }

    template <typename T>
    inline T zalloc_page() {
        T x = reinterpret_cast<T>(PMEM::alloc_page(VMM::PTE_PRESENT | VMM::PTE_WRITABLE | VMM::PTE_CACHELESS));
        if (!x) {
            Debug::krnl_print("xHCI", Debug::LOG_ERROR, "PMEM failed to allocate a page!");
            return nullptr;
        }
        memset(x, 0, PAGE_SIZE);
        return x;
    }

    template <typename T>
    inline T zfalloc(size_t mall) {
        size_t total_bytes = mall * sizeof(T);
        T x = reinterpret_cast<T>(KMEM::malloc(mall * sizeof(T)));
        if (!x) {
            Debug::krnl_print("xHCI", Debug::LOG_ERROR, "Out of memory? KMEM return nullptr!");
            return nullptr;
        }
        memset(x, 0, total_bytes);
        return x;
    }

    inline uint8_t xHCI::GetPortSpeed(uint32_t portsc) {
        return (portsc >> xHCI::XHCI_PORTSC_SPEED_SHIFT) & XHCI_PORTSC_SPEED_MASK;
    }

    void xHCI::addr_device(uint8_t slot_id, int root_port_index) {
        pending_port_setup = -1;

        Debug::krnl_print("xHCI", Debug::LOG_INFO, "Addressing device on Slot %i", slot_id);

        DeviceContext *output_ctx = zalloc_page<DeviceContext *>();
        if (!output_ctx)
            panic(PanicReasons::xHCI_CRITICAL_ERROR);
        uint64_t output_phys = VMM::GetPhysicalAddress(read_cr3(), (uint64_t)output_ctx);
        dcbaap_virt[slot_id] = output_phys;

        InputControlContext* input_ctx = zalloc_page<InputControlContext *>();
        if (!input_ctx)
            panic(PanicReasons::xHCI_CRITICAL_ERROR);
        uint64_t input_phys = VMM::GetPhysicalAddress(read_cr3(), (uint64_t)input_ctx);

        uint64_t op_base = (uint64_t)op_regs;
        uint64_t port_base = op_base + XHCI_PORT_REG_SET_OFFSET + (root_port_index * XHCI_PORT_REG_STRIDE);
        volatile uint32_t* portsc_ptr = (volatile uint32_t*)port_base;

        uint32_t portsc = *portsc_ptr;
        uint8_t speed_id = GetPortSpeed(portsc);

        uint16_t max_packet_size{};
        switch (static_cast<xHCISpeedID>(speed_id)) {
            case xHCISpeedID::SuperSpeed:
            case xHCISpeedID::SuperSpeedPlus:
                max_packet_size = USB_EP0_MAX_PACKET_SUPERSPEED;
                break;
            case xHCISpeedID::HighSpeed:
                max_packet_size = USB_EP0_MAX_PACKET_HIGHSPEED;
                break;
            default:
                max_packet_size = USB_EP0_MAX_PACKET_DEFAULT;
                break;
        }

        Debug::krnl_print("xHCI", Debug::LOG_INFO, "-> Port Speed ID: %i\n\t-> MPS: %i", speed_id, max_packet_size);
        
        input_ctx->add_flags = XHCI_INPUT_ADD_SLOT_CONTEXT | XHCI_INPUT_ADD_EP0_CONTEXT;
        uint32_t context_entries = 1; 
        input_ctx->slot.info = 
            (context_entries << XHCI_SLOT_CONTEXT_ENTRIES_SHIFT) | 
            (speed_id << XHCI_SLOT_SPEED_SHIFT);
        uint32_t xhci_port_num = root_port_index + 1;
        input_ctx->slot.info2 = (xhci_port_num << XHCI_SLOT_ROOT_PORT_SHIFT);

        uint64_t *transfer_ring = zalloc_page<uint64_t *>();
        uint64_t tr_phys = VMM::GetPhysicalAddress(read_cr3(), (uint64_t)transfer_ring);

        memset(transfer_ring, 0, PAGE_SIZE);

        size_t max_trbs = PAGE_SIZE / sizeof(TRB); // 256
        TRB *link_trb = reinterpret_cast<TRB*>(&transfer_ring[(max_trbs - 1) * (sizeof(TRB)/8)]); 
        link_trb->param = tr_phys;
        link_trb->control = (TRB_TYPE_LINK << XHCI_TRB_TYPE_SHIFT) | XHCI_TRB_LINK_TC | (1U << XHCI_TRB_CYCLE_SHIFT);
        
        ep_rings[slot_id][XHCI_EP_INDEX_0] = transfer_ring;
        ep_cycle_states[slot_id][XHCI_EP_INDEX_0] = 1;
        ep_enqueue_ptrs[slot_id][XHCI_EP_INDEX_0] = 0;
        ep_ring_physs[slot_id][XHCI_EP_INDEX_0] = tr_phys;

        uint32_t ep_type = static_cast<uint32_t>(xHCIEpType::Control);

        input_ctx->endpoints[XHCI_CTX_EP0_INDEX].ep_info2 = 
            (XHCI_EP_MAX_ERROR_COUNT << XHCI_EP_CERR_SHIFT) |
            (ep_type << XHCI_EP_TYPE_SHIFT) |
            (max_packet_size << XHCI_EP_MAX_PACKET_SHIFT);

        input_ctx->endpoints[XHCI_CTX_EP0_INDEX].deq_ptr = tr_phys | XHCI_DEQ_CYCLE_STATE_START;

        send_command(TRB_TYPE_ADDRESS_DEVICE, input_phys, slot_id);
        return;
    }

    void xHCI::send_control_request(uint8_t slot_id, uint8_t requestType, uint8_t request, 
        uint16_t value, uint16_t index, uint16_t length, uint64_t buffer_phys) {
        USBSetupPacket setup;
        setup.request_type = requestType;
        setup.request = request;
        setup.value = value;
        setup.index = index;
        setup.length = length;

        uint64_t setup_value_raw{};
        memcpy(&setup_value_raw, &setup, sizeof(uint64_t));
        uint32_t trt = static_cast<uint32_t>(xHCITransferType::NoDataStage);

        if (length > 0) {
            bool is_input = (requestType & USB_REQUEST_DIR_IN) != 0;
            trt = is_input ? static_cast<uint32_t>(xHCITransferType::InDataStage) 
                : static_cast<uint32_t>(xHCITransferType::OutDataStage);
        }

        TRB *ring_base = (TRB*)ep_rings[slot_id][XHCI_EP_INDEX_0];
        uint64_t enqueue = ep_enqueue_ptrs[slot_id][XHCI_EP_INDEX_0];
        uint8_t cycle = ep_cycle_states[slot_id][XHCI_EP_INDEX_0];

        TRB *setup_trb = &ring_base[enqueue];
        setup_trb->param = setup_value_raw;
        setup_trb->status = USB_SETUP_PACKET_SIZE;
        uint32_t trb_type = static_cast<uint32_t>(xHCITrbType::SetupStage);
        setup_trb->control = 
            (trb_type << XHCI_TRB_TYPE_SHIFT) |
            (trt << XHCI_TRB_TRT_SHIFT)       |
            (1U << XHCI_TRB_IDT_SHIFT)        |
            ((cycle ? 1U : 0U) << XHCI_TRB_CYCLE_SHIFT);
        
        enqueue++;

        if (enqueue >= XHCI_CMD_RING_INDEX_LIMIT) {
            TRB *link = &ring_base[XHCI_CMD_RING_INDEX_LIMIT];
            link->param  = ep_ring_physs[slot_id][XHCI_EP_INDEX_0];
            link->status = 0;
            
            link->control = (TRB_TYPE_LINK << XHCI_TRB_TYPE_SHIFT) | 
                            XHCI_TRB_LINK_TC | 
                            ((cycle ? 1U : 0U) << XHCI_TRB_CYCLE_SHIFT);

            Debug::krnl_print("xHCI", Debug::LOG_INFO, "Hit xHCI ring index limit during main section (before length > 0) Send Control Request!");

            asm volatile("sfence" ::: "memory"); 
            asm volatile("mfence" ::: "memory");
            
            enqueue = 0;
            cycle = !cycle;
        }

        if (length > 0) {
            TRB *data_trb = &ring_base[enqueue];

            data_trb->param = buffer_phys;
            data_trb->status = length;

            bool is_input = (requestType & USB_REQUEST_DIR_IN) != 0;
            uint32_t dir_flag = is_input ? XHCI_TRB_DATA_DIR_IN : XHCI_TRB_DATA_DIR_OUT;

            uint32_t trb_type = static_cast<uint32_t>(xHCITrbType::DataStage);

            data_trb->control = 
                (trb_type << XHCI_TRB_TYPE_SHIFT) | 
                (dir_flag << XHCI_TRB_DIR_SHIFT)  | 
                ((cycle ? 1U : 0U) << XHCI_TRB_CYCLE_SHIFT);

            enqueue++;

            if (enqueue >= XHCI_CMD_RING_INDEX_LIMIT) {
                TRB *link = &ring_base[XHCI_CMD_RING_INDEX_LIMIT];
                link->param  = ep_ring_physs[slot_id][XHCI_EP_INDEX_0];
                link->status = 0;

                link->control = (TRB_TYPE_LINK << XHCI_TRB_TYPE_SHIFT) | 
                                XHCI_TRB_LINK_TC | 
                                ((cycle ? 1U : 0U) << XHCI_TRB_CYCLE_SHIFT);

                Debug::krnl_print("xHCI", Debug::LOG_INFO, "Hit xHCI ring index limit during (length > 0) Send Control Request!");

                asm volatile("sfence" ::: "memory"); 
                asm volatile("mfence" ::: "memory");

                enqueue = 0;
                cycle = !cycle;
            }
        }

        uint32_t status_dir = XHCI_TRB_STATUS_DIR_OUT;
        if (length == 0 || (requestType & USB_REQUEST_DIR_IN) == 0) {
            status_dir = XHCI_TRB_STATUS_DIR_IN;
        }

        TRB *status_trb = &ring_base[enqueue];
        status_trb->param = 0;
        status_trb->status = 0;

        trb_type = static_cast<uint32_t>(xHCITrbType::StatusStage);

        status_trb->control = 
            (trb_type << XHCI_TRB_TYPE_SHIFT) | 
            (status_dir << XHCI_TRB_STATUS_DIR_SHIFT) | 
            (1U << XHCI_TRB_IOC_SHIFT) | 
            ((cycle ? 1U : 0U) << XHCI_TRB_CYCLE_SHIFT);

        enqueue++;

        if (enqueue >= XHCI_CMD_RING_INDEX_LIMIT) {
            TRB *link = &ring_base[XHCI_CMD_RING_INDEX_LIMIT];
            link->param  = ep_ring_physs[slot_id][XHCI_EP_INDEX_0];
            link->status = 0;

            link->control = (TRB_TYPE_LINK << XHCI_TRB_TYPE_SHIFT) | 
                            XHCI_TRB_LINK_TC | 
                            ((cycle ? 1U : 0U) << XHCI_TRB_CYCLE_SHIFT);

            Debug::krnl_print("xHCI", Debug::LOG_INFO, "Hit xHCI ring index limit during last section of Send Control Request!");

            asm volatile("sfence" ::: "memory"); 
            asm volatile("mfence" ::: "memory");

            enqueue = 0;
            cycle = !cycle;
        }

        asm volatile("sfence" ::: "memory"); 
        asm volatile("mfence" ::: "memory");

        ep_enqueue_ptrs[slot_id][XHCI_EP_INDEX_0] = enqueue;
        ep_cycle_states[slot_id][XHCI_EP_INDEX_0] = cycle;

        db_regs[slot_id].value = XHCI_DB_TARGET_EP0;

        return;
    }

    void xHCI::initialize() {
        Debug::krnl_print("xHCI", Debug::LOG_INFO, "initialize");

        port_slot_mapping = zfalloc<uint8_t *>(USB_TOTAL_xHCI_SLOTS);
        
        ep_rings = zfalloc<uint64_t ***>(USB_TOTAL_xHCI_SLOTS);
        ep_cycle_states = zfalloc<uint8_t **>(USB_TOTAL_xHCI_SLOTS);
        ep_enqueue_ptrs = zfalloc<uint64_t **>(USB_TOTAL_xHCI_SLOTS);
        ep_ring_physs = zfalloc<uint64_t **>(USB_TOTAL_xHCI_SLOTS);
        config_descriptor_buffer = zfalloc<uint8_t **>(USB_TOTAL_xHCI_SLOTS);
        descriptor_buffer = zfalloc<void **>(USB_TOTAL_xHCI_SLOTS);
        pending_config_value = zfalloc<uint8_t *>(USB_TOTAL_xHCI_SLOTS);

        for(auto i{0uz}; i < USB_TOTAL_xHCI_SLOTS; i++) {
            ep_rings[i] = zfalloc<uint64_t **>(DEVICE_CONTEXT_ENTRIES);
            ep_cycle_states[i] = zfalloc<uint8_t *>(DEVICE_CONTEXT_ENTRIES);
            ep_enqueue_ptrs[i] = zfalloc<uint64_t *>(DEVICE_CONTEXT_ENTRIES);
            ep_ring_physs[i] = zfalloc<uint64_t *>(DEVICE_CONTEXT_ENTRIES);
        }

        attached_drivers = zfalloc<xHCIDriver **>(USB_TOTAL_xHCI_SLOTS);
        slot_states = zfalloc<SetupState *>(USB_TOTAL_xHCI_SLOTS);

        mmio_base_phys = PCI::GetBAR(pci_bus, pci_device, pci_func, BAR_INDEX_0);

        if (!mmio_base_phys) {
            Debug::krnl_print("xHCI", Debug::LOG_WARN, "PCI BAR0 is 0. xHCI initialization aborted.");
            panic(PanicReasons::xHCI_CRITICAL_ERROR);
        }

        mmio_base_virt = (uint64_t)PMEM::map_mmio(mmio_base_phys, MMIO_MAP_PAGES);
        PCI::EnableBusMaster(pci_bus, pci_device, pci_func);

        volatile uint32_t cap_length_reg = *reinterpret_cast<volatile uint32_t*>(mmio_base_virt);
        uint8_t cap_length = static_cast<uint8_t>(cap_length_reg & 0xFF);

        auto* hcs_params_ptr = reinterpret_cast<volatile uint32_t*>(mmio_base_virt + XHCI_CAP_HCSPARAMS1);
        uint32_t hcs_params1 = *hcs_params_ptr;

        max_ports = (hcs_params1 >> XHCI_HCSPARAMS1_MAX_PORTS_SHIFT) & XHCI_HCSPARAMS1_MAX_PORTS_MASK;

        cap_regs = reinterpret_cast<CapabilityRegisters *>(mmio_base_virt);
        op_regs = reinterpret_cast<OperationalRegisters *>(mmio_base_virt + cap_length);
        run_regs = reinterpret_cast<RuntimeRegisters *>(mmio_base_virt + cap_regs->rts_off);
        db_regs = reinterpret_cast<DoorbellRegister *>(mmio_base_virt + cap_regs->db_off);

        Debug::krnl_print("xHCI", Debug::LOG_INFO, "Executing BIOS handoff...");
        bios_handoff();
        Debug::krnl_print("xHCI", Debug::LOG_INFO, "Resetting controller...");
        reset_controller();
        Debug::krnl_print("xHCI", Debug::LOG_INFO, "Enabling MSIX...");
        PCI::EnableMSIX(pci_bus, pci_device, pci_func, IDT::MSIX_VECTOR, ACPI::get_apic_id());

        Debug::krnl_print("xHCI", Debug::LOG_INFO, "Allocating DCBAAP...");
        op_regs->config = max_slots;
        dcbaap_virt = zalloc_page<uint64_t *>();
        op_regs->dcbaap = VMM::GetPhysicalAddress(read_cr3(), (uint64_t)dcbaap_virt);

        Debug::krnl_print("xHCI", Debug::LOG_INFO, "Allocating Command Ring...");

        cmd_ring_virt = zalloc_page<uint64_t *>();
        uint64_t cmd_ring_phys = VMM::GetPhysicalAddress(read_cr3(), (uint64_t)cmd_ring_virt);
        op_regs->crcr = cmd_ring_phys | XHCI_CRCR_RCS;

        event_ring_virt = zalloc_page<uint64_t *>();
        uint64_t event_ring_phys = VMM::GetPhysicalAddress(read_cr3(), (uint64_t)event_ring_virt);

        erst_virt = zalloc_page<EventRingSegmentTableEntry *>();
        erst_virt[XHCI_ERST_PRIMARY_SEGMENT].base_address = event_ring_phys;
        erst_virt[XHCI_ERST_PRIMARY_SEGMENT].size = XHCI_EVENT_RING_TRBS;

        uint64_t erst_phys = VMM::GetPhysicalAddress(read_cr3(), (uint64_t)erst_virt);

        run_regs->irs[XHCI_PRIMARY_INTERRUPTER].erst_size = XHCI_ERST_SINGLE_SEGMENT;
        run_regs->irs[XHCI_PRIMARY_INTERRUPTER].erst_ba = erst_phys;
        run_regs->irs[XHCI_PRIMARY_INTERRUPTER].erdp = event_ring_phys;

        run_regs->irs[XHCI_PRIMARY_INTERRUPTER].iman |= (XHCI_IMAN_IP | XHCI_IMAN_IE);
        run_regs->irs[XHCI_PRIMARY_INTERRUPTER].imod = XHCI_IMOD_1MS_INTERVAL;

        Debug::krnl_print("xHCI", Debug::LOG_INFO, "Starting controller");

        op_regs->usb_cmd = (XHCI_CMD_RUN_STOP | XHCI_CMD_INTE | XHCI_CMD_HSEE);
        while (op_regs->usb_sts & XHCI_STS_HCHALTED) {
            asm volatile("pause");
        }

        cmd_ring_base = (TRB *)cmd_ring_virt;
        cmd_ring_enqueue_ptr = 0;
        cmd_ring_cycle_state = XHCI_COMMAND_RING_CYCLE_START;

        Debug::krnl_print("xHCI", Debug::LOG_INFO, "Initialization Complete. Checking Ports...");
        check_ports();

        MSIX::xHCI::register_xhci_worker(this);
    }

    void xHCI::reset_controller() {
        if (op_regs->usb_cmd & XHCI_CMD_RUN_STOP) {
            op_regs->usb_cmd &= ~XHCI_CMD_RUN_STOP;
            
            while (!(op_regs->usb_sts & XHCI_STS_HCHALTED)) {
                asm volatile("pause");
            }
        }

        op_regs->usb_cmd |= XHCI_CMD_HCRST;

        while (op_regs->usb_cmd & XHCI_CMD_HCRST) {
            asm volatile("pause");
        }

        while (op_regs->usb_sts & XHCI_STS_CNR) {
            asm volatile("pause");
        }
    }

    void xHCI::send_command(uint32_t type, uint64_t param, uint32_t slot_id_or_status) {
        TRB *command = &cmd_ring_base[cmd_ring_enqueue_ptr];
        command->param = param;

        uint32_t control = (type << XHCI_TRB_TYPE_SHIFT);

        if (type == TRB_TYPE_ADDRESS_DEVICE || type == TRB_TYPE_CONFIGURE_EP) {
        command->status = 0;
            control |= (slot_id_or_status << XHCI_TRB_SLOT_ID_SHIFT);
        } else {
            command->status = slot_id_or_status;
        }

        if (cmd_ring_cycle_state) {
            control |= (1U << XHCI_TRB_CYCLE_SHIFT);
        } else {
            control &= ~(1U << XHCI_TRB_CYCLE_SHIFT);
        }

        command->control = control;

        cmd_ring_enqueue_ptr++;

        if (cmd_ring_enqueue_ptr == XHCI_CMD_RING_INDEX_LIMIT) {
            TRB *link = &cmd_ring_base[XHCI_CMD_RING_INDEX_LIMIT];
            
            link->param = VMM::GetPhysicalAddress(read_cr3(), (uint64_t)cmd_ring_base); 
            link->status = 0;
            
            link->control = (TRB_TYPE_LINK << XHCI_TRB_TYPE_SHIFT) | 
                            XHCI_TRB_LINK_TC | 
                            ((cmd_ring_cycle_state ? 1U : 0U) << XHCI_TRB_CYCLE_SHIFT);
            
            Debug::krnl_print("xHCI", Debug::LOG_INFO, "Hit xHCI cmd ring index limit during send_command.");

            asm volatile("sfence" ::: "memory"); 
            asm volatile("mfence" ::: "memory");

            cmd_ring_enqueue_ptr = 0;
            cmd_ring_cycle_state = !cmd_ring_cycle_state;
        }

        asm volatile("sfence" ::: "memory"); 
        asm volatile("mfence" ::: "memory");

        db_regs[XHCI_HC_DOORBELL].value = XHCI_DB_TARGET_HC;
    }

    bool xHCI::poll_event_ring() {
        TRB *event_ring = reinterpret_cast<TRB *>(event_ring_virt);
        bool events_proc{};
        
        for (auto i{0uz}; i < XHCI_MAX_EVENTS_PER_INTR; i++) {
            TRB *event = &event_ring[event_ring_dequeue_ptr];

            if ((event->control & XHCI_TRB_CYCLE_MASK) != event_ring_cycle_state) {
                break;
            }

            events_proc = true;

            uint32_t type = (event->control >> XHCI_TRB_TYPE_SHIFT) & XHCI_TRB_TYPE_MASK;
            uint32_t slot = (event->control >> XHCI_TRB_SLOT_ID_SHIFT) & XHCI_TRB_SLOT_ID_MASK;

            if (type == TRB_TYPE_CMD_COMPLETE) {
                uint32_t code = (event->status >> XHCI_TRB_COMP_CODE_SHIFT) & XHCI_TRB_COMP_CODE_MASK;
                
                uint8_t allocated_slot = (event->control >> XHCI_TRB_SLOT_ID_SHIFT) & XHCI_TRB_SLOT_ID_MASK;
                if (allocated_slot == 0) {
                    for (uint32_t s = 1; s < USB_TOTAL_xHCI_SLOTS; s++) {
                        if (slot_states[s] == SetupState::STATE_CONFIGURING_EP ||
                            slot_states[s] == SetupState::STATE_ADDRESSING) {
                            allocated_slot = s;
                            break;
                        }
                    }
                }

                if (code == XHCI_CODE_SUCCESS && allocated_slot != 0) {
                    TRB *completed_cmd_trb = reinterpret_cast<TRB *>(event->param + PMM::hhdm_offset);
                    uint8_t cmd_type = (completed_cmd_trb->control >> XHCI_TRB_TYPE_SHIFT) & XHCI_TRB_TYPE_MASK;


                    asm volatile("cli");
                    if (cmd_type == TRB_TYPE_ENABLE_SLOT) {
                        Debug::krnl_print("xHCI", Debug::LOG_INFO, "Slot assigned: %i", allocated_slot);

                        slot_states[allocated_slot] = SetupState::STATE_ADDRESSING;
                        port_slot_mapping[pending_port_setup] = allocated_slot;

                        addr_device(allocated_slot, pending_port_setup);
                        pending_port_setup = PENDING_PORT_COMPLETE;
                        Debug::krnl_print("xHCI", Debug::LOG_INFO, "Setting IF");
                    }
                    else if (cmd_type == TRB_TYPE_ADDRESS_DEVICE) {
                        descriptor_buffer[allocated_slot] = zfalloc<uint8_t *>(USB::DEVICE_DESCRIPTOR_SIZE);
                        uint64_t descriptor_phys = VMM::GetPhysicalAddress(read_cr3(), (uint64_t)descriptor_buffer[allocated_slot]);
                        slot_states[allocated_slot] = SetupState::STATE_GET_DEV_DESC;

                        uint8_t req_type = USB::REQ_DIR_IN | USB::REQ_TYPE_STANDARD | USB::REQ_REC_DEVICE;
                        uint16_t wValue  = USB::make_wValue(USB::DESC_TYPE_DEVICE, 0);
                        send_control_request(allocated_slot, req_type, USB::REQ_GET_DESCRIPTOR, wValue, 0, USB::DEVICE_DESCRIPTOR_SIZE, descriptor_phys);
                    }
                    else if (cmd_type == TRB_TYPE_CONFIGURE_EP) {
                        Debug::krnl_print("xHCI", Debug::LOG_INFO, "Endpoints Configured. Setting Device Configuration...");
                        slot_states[allocated_slot] = SetupState::STATE_SETTING_CONFIG;

                        uint8_t req_type = USB::REQ_DIR_OUT | USB::REQ_TYPE_STANDARD | USB::REQ_REC_DEVICE;
                        uint16_t config_val = pending_config_value[allocated_slot] ? pending_config_value[allocated_slot] : USB::DEFAULT_CONFIGURATION_VALUE;

                        send_control_request(allocated_slot, req_type, USB::REQ_SET_CONFIGURATION, config_val, 0, 0, 0);
                    }

                    if (slot_states[allocated_slot] == SetupState::STATE_CONFIGURED) {
                        if (attached_drivers[allocated_slot]) {
                            check_ports();
                        }
                    }

                    asm volatile("sti");
                }
                else {
                    Debug::krnl_print("xHCI", Debug::LOG_WARN, "Command failed. Code: %x", code);
                }
            }
            else if (type == TRB_TYPE_PORT_STATUS) {
                Debug::krnl_print("xHCI", Debug::LOG_INFO, "Port Status Change");
                check_ports();
            }
            else if (type == TRB_TRANSFER_EVENT) {
                uint32_t transfer_len   = event->status & TRB_TRANSFER_LEN_MASK;
                uint32_t completion_code = (event->status >> XHCI_TRB_COMP_CODE_SHIFT) & XHCI_TRB_COMP_CODE_MASK;
                if (completion_code == XHCI_COMP_SUCCESS || completion_code == XHCI_COMP_SHORT_PACKET) {
                    switch (slot_states[slot]) {
                        case SetupState::STATE_GET_DEV_DESC: {
                            KMEM::free(descriptor_buffer[slot]);
                            descriptor_buffer[slot] = nullptr;
                            config_descriptor_buffer[slot] = zfalloc<uint8_t *>(CONFIG_HEADER_SIZE);
                            uint64_t phys = VMM::GetPhysicalAddress(read_cr3(), (uint64_t)config_descriptor_buffer[slot]);
                            slot_states[slot] = SetupState::STATE_GET_CONFIG_DESC_HEADER;
                
                            uint8_t  req_type = USB::REQ_DIR_IN | USB::REQ_TYPE_STANDARD | USB::REQ_REC_DEVICE;
                            uint16_t wValue   = USB::make_wValue(USB::DESC_TYPE_CONFIG, 0);

                            send_control_request(slot, req_type, USB::REQ_GET_DESCRIPTOR, wValue, 0x0000, CONFIG_HEADER_SIZE, phys);
                            break;
                        }
                        case SetupState::STATE_GET_CONFIG_DESC_HEADER: {
                            auto *cfg = reinterpret_cast<USBConfigDescriptor*>(config_descriptor_buffer[slot]);
                            uint16_t total_len = cfg->wTotalLength;
                            KMEM::free(config_descriptor_buffer[slot]);
                            config_descriptor_buffer[slot] = zfalloc<uint8_t *>(total_len);
                            uint64_t phys = VMM::GetPhysicalAddress(read_cr3(), (uint64_t)config_descriptor_buffer[slot]);
                            slot_states[slot] = SetupState::STATE_GET_CONFIG_DESC_FULL;

                            uint8_t  req_type = USB::REQ_DIR_IN | USB::REQ_TYPE_STANDARD | USB::REQ_REC_DEVICE;
                            uint16_t wValue   = USB::make_wValue(USB::DESC_TYPE_CONFIG, 0);
                            send_control_request(slot, req_type, USB::REQ_GET_DESCRIPTOR, wValue, 0x0000, total_len, phys);
                            break;
                        }
                        case SetupState::STATE_GET_CONFIG_DESC_FULL: {
                            auto *cfg = reinterpret_cast<USBConfigDescriptor*>(config_descriptor_buffer[slot]);
                            uint8_t *walker = config_descriptor_buffer[slot] + cfg->bLength;
                            uint8_t *end = config_descriptor_buffer[slot] + cfg->wTotalLength;

                            xHCIDriver* matched_driver = nullptr;
                            ParsedEndpoint endpoints[MAX_PARSED_ENDPOINTS]; 
                            int ep_count = 0;

                            while (walker < end) {
                                uint8_t length    = walker[0];
                                uint8_t desc_type = walker[1];

                                if (desc_type == USB::DESC_TYPE_INTERFACE) { 
                                    if (matched_driver) break;
                                    auto* intf = reinterpret_cast<USBInterfaceDescriptor *>(walker);
                                    matched_driver = LoadNewDriver(
                                        intf->bInterfaceClass, intf->bInterfaceSubClass, intf->bInterfaceProtocol
                                    );
                                }
                                else if (desc_type == USB::DESC_TYPE_ENDPOINT && matched_driver) {
                                    if (ep_count < MAX_PARSED_ENDPOINTS) {
                                        auto* ep_desc = reinterpret_cast<USBEndpointDescriptor *>(walker);
                                        endpoints[ep_count].address          = ep_desc->bEndpointAddress;
                                        endpoints[ep_count].attributes       = ep_desc->bmAttributes;
                                        endpoints[ep_count].max_packet_size  = ep_desc->wMaxPacketSize;
                                        ep_count++;
                                    }
                                }
                            
                                walker += length;
                            }

                            if (matched_driver && ep_count > 0) {
                                Debug::krnl_print("xHCI", Debug::LOG_INFO, "-> Driver match found! Endpoints to configure: %i", ep_count);
                                attached_drivers[slot] = matched_driver;
                                matched_driver->initialize(this, slot, endpoints, ep_count);
                                conf_interface_endpoints(slot, endpoints, ep_count);
                            }

                            KMEM::free(config_descriptor_buffer[slot]);
                            config_descriptor_buffer[slot] = nullptr;
                            break;
                        }
                        case SetupState::STATE_CONFIGURED: {
                            if (attached_drivers[slot]) {
                                uint32_t dci = (event->control >> ENDPOINT_ID_SHIFT) & 0x1F;
                                
                                uint8_t ep_num = dci / 2;
                                bool is_in = (dci % 2) != 0;
                                uint8_t raw_ep_addr = ep_num | (is_in ? USB::USB_EP_DIR_IN : 0);
                                
                                attached_drivers[slot]->on_int(transfer_len, raw_ep_addr, event->param);
                                }
                            break;
                        }
                        case SetupState::STATE_SETTING_CONFIG: {
                            slot_states[slot] = SetupState::STATE_CONFIGURED;
                            Debug::krnl_print("xHCI", Debug::LOG_INFO, "Device fully configured & ready!");

                            while (proc_id > 0) asm volatile("pause");

                            proc_id = slot;
                            char _name[16];
                            Debug::snprintf(_name, 16, "xHCI Subtask %i", slot);
                            Scheduler::Task *res = new Scheduler::Task(
                                (Scheduler::Task::EntryPoint)[](void *xHCI_ptr) {
                                    Debug::krnl_print("xHCI", Debug::LOG_INFO, "Subtask entry");
                                    xHCI *ptr = static_cast<xHCI *>(xHCI_ptr);
                                    int ptr_id = ptr->proc_id;
                                    ptr->proc_id = 0;
                                    ptr->attached_drivers[ptr_id]->start();

                                    Scheduler::Suicide();

                                    while (true);
                                },
                                _name,
                                true,
                                this
                            );

                            res->core_pinned = true;
                            res->current_core = 0;
                        
                            check_ports();
                            break;
                        }

                        default:
                            break;
                    }
                }
                else {
                    Debug::krnl_print("xHCI", Debug::LOG_WARN, "Transfer error on slot %i, with code: %x", slot, completion_code);
                }
            }

            event_ring_dequeue_ptr++;
            if (event_ring_dequeue_ptr >= XHCI_EVENT_RING_TRBS) {
                event_ring_dequeue_ptr = XHCI_ERST_PRIMARY_SEGMENT;
                event_ring_cycle_state = !event_ring_cycle_state;
            }
        }

        if (events_proc) {
            uint64_t event_ring_base_phys = erst_virt[XHCI_ERST_PRIMARY_SEGMENT].base_address;
            uint64_t erdp_phys = event_ring_base_phys + (event_ring_dequeue_ptr * XHCI_TRB_SIZE_BYTES);

            uint64_t current_erdp = run_regs->irs[XHCI_PRIMARY_INTERRUPTER].erdp;
            erdp_phys &= ~0x0FULL;
            run_regs->irs[XHCI_PRIMARY_INTERRUPTER].erdp = erdp_phys | (current_erdp & 0xF) | XHCI_ERDP_EHB;
            run_regs->irs[XHCI_PRIMARY_INTERRUPTER].iman |= XHCI_IMAN_IP;
        }

        asm volatile("sfence" ::: "memory"); 
        asm volatile("mfence" ::: "memory");


        return events_proc;
    }

    void *stat_addr = nullptr;
    void xHCI::conf_endpoint(uint8_t slot_id, uint8_t endpoint_address, uint8_t endpoint_type, uint16_t max_packet_size) {
        uint8_t ep_num = endpoint_address & USB::USB_EP_NUM_MASK;
        bool is_in = (endpoint_address & USB::USB_EP_DIR_IN) != 0;
        uint8_t dci = (ep_num * 2) + (is_in ? 1 : 0);

        Debug::krnl_print("xHCI", Debug::LOG_INFO, "Configuring endpoint DCI: %i", dci);

        if (!stat_addr) {
            stat_addr = zalloc_page<InputControlContext *>();
        }
        auto* input_ctx = static_cast<InputControlContext *>(stat_addr);
        uint64_t input_phys = VMM::GetPhysicalAddress(read_cr3(), reinterpret_cast<uint64_t>(input_ctx));
        input_ctx->add_flags = XHCI_INPUT_ADD_SLOT_CONTEXT | (1U << dci);
        input_ctx->slot.info = (dci << XHCI_SLOT_CONTEXT_ENTRIES_SHIFT);

        auto* ring_virt = zalloc_page<uint64_t *>();
        uint64_t ring_phys = VMM::GetPhysicalAddress(read_cr3(),reinterpret_cast<uint64_t>(ring_virt));

        ep_rings[slot_id][dci]        = ring_virt;
        ep_cycle_states[slot_id][dci] = XHCI_COMMAND_RING_CYCLE_START;
        ep_enqueue_ptrs[slot_id][dci] = 0;
        ep_ring_physs[slot_id][dci]   = ring_phys;

        uint8_t type_base   = is_in ? XHCI_EP_TYPE_IN_SHIFT : XHCI_EP_TYPE_OUT_SHIFT;
        uint8_t xhci_ep_type = type_base + (endpoint_type & USB::USB_EP_TYPE_MASK);

        volatile EndpointContext* ep_ctx = &input_ctx->endpoints[dci - 1];

        ep_ctx->ep_info2 = (static_cast<uint32_t>(xhci_ep_type) << XHCI_EP_TYPE_SHIFT) | 
                       (static_cast<uint32_t>(max_packet_size) << XHCI_EP_MAX_PACKET_SHIFT);

        ep_ctx->ep_info2 |= (XHCI_EP_CERR_MAX << XHCI_EP_CERR_SHIFT);

        ep_ctx->deq_ptr = ring_phys | XHCI_DEQ_CYCLE_STATE_START;

        ep_ctx->tx_info = XHCI_EP_DEFAULT_TRB_LEN;

        slot_states[slot_id] = SetupState::STATE_CONFIGURING_EP;
        send_command(TRB_TYPE_CONFIGURE_EP, input_phys, slot_id);

        asm volatile("sfence" ::: "memory"); 
        asm volatile("mfence" ::: "memory");

        // while(slot_states[slot_id] == SetupState::STATE_CONFIGURING_EP) {
        //     asm volatile("pause");
        // }

        // PMEM::free_page(input_ctx);
    }

    void *cie_addr{nullptr};
    void xHCI::conf_interface_endpoints(uint8_t slot_id, ParsedEndpoint* endpoints, int ep_count) {
        if (!cie_addr) {
            cie_addr = zalloc_page<void *>();
        }
        auto* input_ctx = static_cast<InputControlContext*>(cie_addr);
        uint64_t input_phys = VMM::GetPhysicalAddress(read_cr3(), reinterpret_cast<uint64_t>(input_ctx));
        input_ctx->add_flags = XHCI_INPUT_ADD_SLOT_CONTEXT; 
        uint32_t highest_dci = 1;

        for (int i = 0; i < ep_count; i++) {
            uint8_t ep_num = endpoints[i].address & USB::USB_EP_NUM_MASK;
            bool is_in     = (endpoints[i].address & USB::USB_EP_DIR_IN) != 0;
            uint8_t dci    = (ep_num * 2) + (is_in ? 1 : 0);

            if (dci > highest_dci) highest_dci = dci;

            input_ctx->add_flags |= (1U << dci);

            auto* ring_virt = reinterpret_cast<uint64_t*>(zalloc_page<void*>());
            uint64_t ring_phys = VMM::GetPhysicalAddress(read_cr3(), reinterpret_cast<uint64_t>(ring_virt));

            ep_rings[slot_id][dci]        = ring_virt;
            ep_cycle_states[slot_id][dci] = XHCI_COMMAND_RING_CYCLE_START;
            ep_enqueue_ptrs[slot_id][dci] = 0;
            ep_ring_physs[slot_id][dci]   = ring_phys;

            uint8_t usb_type     = endpoints[i].attributes & USB::USB_EP_TYPE_MASK;
            uint8_t type_base    = is_in ? XHCI_EP_TYPE_IN_SHIFT : XHCI_EP_TYPE_OUT_SHIFT;
            uint8_t xhci_ep_type = type_base + usb_type;

            volatile EndpointContext* ep_ctx = &input_ctx->endpoints[dci - 1];
            
            ep_ctx->ep_info2 = (static_cast<uint32_t>(xhci_ep_type) << XHCI_EP_TYPE_SHIFT) | 
                               (static_cast<uint32_t>(endpoints[i].max_packet_size) << XHCI_EP_MAX_PACKET_SHIFT);
            
            ep_ctx->ep_info2 |= (XHCI_EP_CERR_MAX << XHCI_EP_CERR_SHIFT); 
            ep_ctx->deq_ptr   = ring_phys | XHCI_DEQ_CYCLE_STATE_START;
            ep_ctx->tx_info   = XHCI_EP_DEFAULT_TRB_LEN;

            asm volatile("sfence" ::: "memory"); 
            asm volatile("mfence" ::: "memory");
        }

        int root_port = 0;
        for (int i = 0; i < max_ports; i++) {
            if (port_slot_mapping[i] == slot_id) {
                root_port = i;
                break;
            }
        }

        uintptr_t op_base_addr = reinterpret_cast<uintptr_t>(op_regs);  
        uintptr_t port_base    = op_base_addr + XHCI_PORT_REG_SET_OFFSET + (root_port *     XHCI_PORT_REG_STRIDE);

        uint32_t portsc  = *reinterpret_cast<volatile uint32_t*>(port_base);
        uint8_t speed_id = (portsc >> XHCI_PORTSC_SPEED_SHIFT) & XHCI_PORTSC_SPEED_MASK;

        input_ctx->slot.info  = (highest_dci << XHCI_SLOT_CONTEXT_ENTRIES_SHIFT) | 
                            (static_cast<uint32_t>(speed_id) << XHCI_SLOT_SPEED_SHIFT);

        input_ctx->slot.info2 = (static_cast<uint32_t>(root_port + 1) << XHCI_SLOT_ROOT_PORT_SHIFT);
        slot_states[slot_id] = SetupState::STATE_CONFIGURING_EP;

        send_command(TRB_TYPE_CONFIGURE_EP, input_phys, slot_id);

        // while(slot_states[slot_id] == SetupState::STATE_CONFIGURING_EP) {
        //     asm volatile("pause");
        // }

        // PMEM::free_page(input_ctx);
    }
    
    void xHCI::check_ports() {
        uintptr_t op_base_addr = reinterpret_cast<uintptr_t>(op_regs);
        for (int i = 0; i < max_ports; i++) {
            uintptr_t port_base = op_base_addr + XHCI_PORT_REG_SET_OFFSET + (i * XHCI_PORT_REG_STRIDE);
            auto* portsc_reg = reinterpret_cast<volatile uint32_t*>(port_base);
            uint32_t status = *portsc_reg;

            bool connected = status & XHCI_PORTSC_CCS;
            bool enabled = status & XHCI_PORTSC_PED;

            if (connected) {
                if (!enabled) {
                    reset_ports(i);
                }
                else if (enabled) {
                    if (port_slot_mapping[i]) {
                       continue;
                    }

                    if (pending_port_setup == XHCI_NO_PENDING_PORT) {
                        Debug::krnl_print("xHCI", Debug::LOG_INFO, "New device on port %i. Requesting Slot...", i + 1);

                        pending_port_setup = i;
                        send_command(TRB_TYPE_ENABLE_SLOT, 0, 0);

                        asm volatile("sfence" ::: "memory"); 
                        asm volatile("mfence" ::: "memory");
                    }
                }
            }
            else {
                if (port_slot_mapping[i]) {
                    port_slot_mapping[i] = 0;
                }
            }
        }
    }

    void xHCI::reset_ports(int port_idx) {
        uintptr_t op_base_addr = reinterpret_cast<uintptr_t>(op_regs);
        uintptr_t port_base    = op_base_addr + XHCI_PORT_REG_SET_OFFSET + (port_idx * XHCI_PORT_REG_STRIDE);
        auto* portsc_reg = reinterpret_cast<volatile uint32_t*>(port_base);

        uint32_t status = *portsc_reg;

        status &= ~XHCI_PORTSC_RW1C_MASK;
        *portsc_reg = (status | XHCI_PORTSC_PR);

        asm volatile("sfence" ::: "memory"); 
        asm volatile("mfence" ::: "memory");
    }

    void xHCI::queue_bulk_transfer(uint8_t slot_id, uint8_t endpoint_address, uint64_t buffer_phys, uint32_t buffer_size) {
        Debug::krnl_print("xHCI", Debug::LOG_INFO, "Queuing a bulk transfer!");
        aquire_lock();
        uint8_t ep_num = endpoint_address & USB::USB_EP_NUM_MASK;
        bool is_in = (endpoint_address & USB::USB_EP_DIR_IN) != 0;
        uint8_t dci = (ep_num * 2) + (is_in ? 1 : 0);

        TRB *ring = reinterpret_cast<TRB*>(ep_rings[slot_id][dci]);
        uint64_t idx = ep_enqueue_ptrs[slot_id][dci];
        uint8_t cycle = ep_cycle_states[slot_id][dci];

        asm volatile("sfence" ::: "memory"); 
        asm volatile("mfence" ::: "memory");

        if (idx == XHCI_CMD_RING_INDEX_LIMIT) {
            TRB *link = &ring[XHCI_CMD_RING_INDEX_LIMIT];
            link->param  = ep_ring_physs[slot_id][dci];
            link->status = 0;
            link->control = (TRB_TYPE_LINK << XHCI_TRB_TYPE_SHIFT) | 
                            XHCI_TRB_LINK_TC | 
                            ((cycle ? 1U : 0U) << XHCI_TRB_CYCLE_SHIFT);

            Debug::krnl_print("xHCI", Debug::LOG_INFO, "Hit xHCI ring index limit during the first check of bulk_transfer");

            idx = 0;
            cycle = !cycle;
        }

        ring[idx].param = buffer_phys;
        ring[idx].status = buffer_size;
        
        uint32_t control_val = (TRB_TYPE_NORMAL << XHCI_TRB_TYPE_SHIFT) | XHCI_TRB_IOC_ENABLE;
        
        ring[idx].control = control_val | ((cycle ? 1U : 0U) << XHCI_TRB_CYCLE_SHIFT);

        idx++;

        if (idx >= XHCI_CMD_RING_INDEX_LIMIT) {
            TRB *link = &ring[XHCI_CMD_RING_INDEX_LIMIT];
            link->param  = ep_ring_physs[slot_id][dci];
            link->status = 0;

            link->control = (TRB_TYPE_LINK << XHCI_TRB_TYPE_SHIFT) | 
                            XHCI_TRB_LINK_TC | 
                            ((cycle ? 1U : 0U) << XHCI_TRB_CYCLE_SHIFT);

            asm volatile("sfence" ::: "memory"); 
            asm volatile("mfence" ::: "memory");

            Debug::krnl_print("xHCI", Debug::LOG_INFO, "Hit xHCI ring index limit during the second check of bulk_transfer");

            idx = 0;
            cycle = !cycle;
        }

        asm volatile("sfence" ::: "memory"); 
        asm volatile("mfence" ::: "memory");

        ep_enqueue_ptrs[slot_id][dci] = idx;
        ep_cycle_states[slot_id][dci] = cycle;

        Debug::krnl_print("xHCI", Debug::LOG_INFO, "Ringing doorbell for slot %i, dci %i", slot_id, dci);

        *(volatile uint32_t*)(&db_regs[slot_id]) = dci;
        release_lock();
        return;
    }

    void xHCI::queue_int_transfer(uint8_t slot_id, uint8_t endpoint_address, uint64_t buffer_phys, uint32_t buffer_size) {
        aquire_lock();
        uint8_t ep_num = endpoint_address & USB::USB_EP_NUM_MASK;
        bool is_in = (endpoint_address & USB::USB_EP_DIR_IN) != 0;
        uint8_t dci = (ep_num * 2) + (is_in ? 1 : 0);

        TRB *ring     = reinterpret_cast<TRB*>(ep_rings[slot_id][dci]);
        uint64_t idx  = ep_enqueue_ptrs[slot_id][dci];
        uint8_t cycle = ep_cycle_states[slot_id][dci];

        if (idx >= XHCI_CMD_RING_INDEX_LIMIT) {
            TRB *link = &ring[XHCI_CMD_RING_INDEX_LIMIT];
            link->param  = ep_ring_physs[slot_id][dci];
            link->status = 0;
            
            link->control = (TRB_TYPE_LINK << XHCI_TRB_TYPE_SHIFT) | 
                            XHCI_TRB_LINK_TC | 
                            ((cycle ? 1U : 0U) << XHCI_TRB_CYCLE_SHIFT);

            asm volatile("sfence" ::: "memory");
            asm volatile("mfence" ::: "memory");

            Debug::krnl_print("xHCI", Debug::LOG_INFO, "Hit xHCI ring index limit during the first check of int_transfer");

            idx = 0;
            cycle = !cycle;
        }

        ring[idx].param  = buffer_phys;
        ring[idx].status = buffer_size;

        asm volatile("sfence" ::: "memory"); 
        asm volatile("mfence" ::: "memory");

        uint32_t control_val = (TRB_TYPE_NORMAL << XHCI_TRB_TYPE_SHIFT) | 
                           XHCI_TRB_IOC_ENABLE | 
                           XHCI_TRB_ISP_ENABLE;

        ring[idx].control = control_val | ((cycle ? 1U : 0U) << XHCI_TRB_CYCLE_SHIFT);

        idx++;

        if (idx >= XHCI_CMD_RING_INDEX_LIMIT) {
            Debug::krnl_print("xHCI", Debug::LOG_INFO, "Started xHCI cmd ring wrap");
            TRB *link = &ring[XHCI_CMD_RING_INDEX_LIMIT];
            link->param  = ep_ring_physs[slot_id][dci];
            link->status = 0;
            
            link->control = (TRB_TYPE_LINK << XHCI_TRB_TYPE_SHIFT) | 
                            XHCI_TRB_LINK_TC | 
                            ((cycle ? 1U : 0U) << XHCI_TRB_CYCLE_SHIFT);

            asm volatile("sfence" ::: "memory"); 
            asm volatile("mfence" ::: "memory");

            Debug::krnl_print("xHCI", Debug::LOG_INFO, "Hit xHCI ring index limit during the second check of int_transfer");

            idx = 0;
            cycle = !cycle;
        }

        asm volatile("sfence" ::: "memory"); 
        asm volatile("mfence" ::: "memory");

        ep_enqueue_ptrs[slot_id][dci] = idx;
        ep_cycle_states[slot_id][dci] = cycle;
        *(volatile uint32_t*)(&db_regs[slot_id]) = dci;
        release_lock();
        return;
    }
}