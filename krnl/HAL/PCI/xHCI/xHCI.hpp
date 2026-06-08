#pragma once
#include <stdint.h>

namespace HAL::PCI {
    struct EventRingSegmentTableEntry {
        uint64_t base_address;
        uint32_t size;
        uint32_t reserved;
    } __attribute__((packed));

    class xHCI {
    public:
        struct ParsedEndpoint {
            uint8_t address;
            uint8_t attributes;
            uint16_t max_packet_size;
        };

        xHCI(uint8_t bus, uint8_t device, uint8_t function);
        ~xHCI();

        void initialize();
        void send_command(uint32_t type, uint64_t param, uint32_t slot_id_or_status);
        void poll_event_ring();
        void check_ports();
        void reset_ports(int port_index);
        void bios_handoff();
        void addr_device(uint8_t slot_id, int root_port_index);

        void send_control_request(uint8_t slot_id, uint8_t requestType, uint8_t request, uint16_t value, uint16_t index, uint16_t length, uint64_t buffer_phys);
        void conf_endpoint(uint8_t slot_id, uint8_t endpoint_address, uint8_t endpoint_type, uint16_t max_packet_size);
        void queue_int_transfer(uint8_t slot_id, uint8_t endpoint_index, uint64_t buffer_phys, uint32_t buffer_size);
        void queue_bulk_transfer(uint8_t slot_id, uint8_t endpoint_address, uint64_t buffer_phys, uint32_t buffer_size);
        void conf_interface_endpoints(uint8_t slot_id, ParsedEndpoint* endpoints, int ep_count);

    private:
        uint8_t pci_bus, pci_device, pci_func;
        uint64_t mmio_base_phys;
        uint64_t mmio_base_virt;

        uint8_t max_slots;
        uint8_t max_ports;

        uint8_t cmd_ring_cycle_state;
        uint64_t cmd_ring_enqueue_ptr;

        uint64_t event_ring_dequeue_ptr;
        uint8_t event_ring_cycle_state;

        int pending_port_setup;

        uint8_t *port_slot_mapping;

        struct CapabilityRegisters {
            uint8_t  cap_length;
            uint8_t  reserved;
            uint16_t hci_version;
            uint32_t hcs_params1;
            uint32_t hcs_params2;
            uint32_t hcs_params3;
            uint32_t hcc_params1;
            uint32_t db_off;
            uint32_t rts_off;
            uint32_t hcc_params2;
        } __attribute__((packed));

        struct PortRegisterSet {
            volatile uint32_t portsc;
            volatile uint32_t portpmsc;
            volatile uint32_t portli;
            volatile uint32_t porthlpmc;
        } __attribute__((packed));

        struct OperationalRegisters {
            volatile uint32_t usb_cmd;
            volatile uint32_t usb_sts;
            volatile uint32_t page_size;
            uint8_t           reserved1[8];
            volatile uint32_t dn_ctrl;
            volatile uint64_t crcr;
            uint8_t           reserved2[16];
            volatile uint64_t dcbaap;
            volatile uint32_t config;
            uint8_t           reserved3[964];
            PortRegisterSet   *ports;
        } __attribute__((packed));

        struct InterrupterRegisterSet {
            volatile uint32_t iman;
            volatile uint32_t imod;
            volatile uint32_t erst_size;
            uint32_t          reserved;
            volatile uint64_t erst_ba;
            volatile uint64_t erdp;
        } __attribute__((packed));

        struct RuntimeRegisters {
            volatile uint32_t microframe_index;
            uint8_t           reserved[28];
            struct InterrupterRegisterSet irs[1024];
        } __attribute__((packed));

        struct DoorbellRegister {
            volatile uint32_t value;
        } __attribute__((packed));

        struct TRB {
            volatile uint64_t param;
            volatile uint32_t status;
            volatile uint32_t control;
        } __attribute__((packed));

        struct EndpointContext {
            volatile uint32_t ep_info;
            volatile uint32_t ep_info2;
            volatile uint64_t deq_ptr;
            volatile uint32_t tx_info;
            volatile uint32_t reserved[3];
        } __attribute__((packed));

        struct SlotContext {
            volatile uint32_t info;
            volatile uint32_t info2;
            volatile uint32_t tt_id;
            volatile uint32_t state;
            volatile uint32_t reserved[4];
        } __attribute__((packed));

        struct DeviceContext {
            SlotContext slot;
            EndpointContext endpoints[31];
        } __attribute__((packed));

        struct InputControlContext {
            volatile uint32_t drop_flags;
            volatile uint32_t add_flags;
            volatile uint32_t reserved[5];
            volatile uint32_t config;

            SlotContext slot;
            EndpointContext endpoints[31];
        } __attribute__((packed));

        struct USBSetupPacket {
            uint8_t  request_type;
            uint8_t  request;
            uint16_t value;
            uint16_t index;
            uint16_t length;
        } __attribute__((packed));

        struct USBDeviceDescriptor {
            uint8_t  length;
            uint8_t  descriptor_type;
            uint16_t bcd_usb;
            uint8_t  device_class;
            uint8_t  device_subclass;
            uint8_t  device_protocol;
            uint8_t  max_packet_size;
            uint16_t id_vendor;
            uint16_t id_product;
            uint16_t bcd_device;
            uint8_t  manufacturer_index;
            uint8_t  product_index;
            uint8_t  serial_index;
            uint8_t  num_configurations;
        } __attribute__((packed));

        struct USBEndpointDescriptor {
            uint8_t  bLength;
            uint8_t  bDescriptorType;
            uint8_t  bEndpointAddress;
            uint8_t  bmAttributes;
            uint16_t wMaxPacketSize;
            uint8_t  bInterval;
        } __attribute__((packed));

        struct USBConfigDescriptor {
            uint8_t  bLength;
            uint8_t  bDescriptorType;
            uint16_t wTotalLength;
            uint8_t  bNumInterfaces;
            uint8_t  bConfigurationValue;
            uint8_t  iConfiguration;
            uint8_t  bmAttributes;
            uint8_t  bMaxPower;
        } __attribute__((packed));

        struct USBInterfaceDescriptor {
            uint8_t bLength;
            uint8_t bDescriptorType;
            uint8_t bInterfaceNumber;
            uint8_t bAlternateSetting;
            uint8_t bNumEndpoints;
            uint8_t bInterfaceClass;
            uint8_t bInterfaceSubClass;
            uint8_t bInterfaceProtocol;
            uint8_t iInterface;
        } __attribute__((packed));


        static constexpr uint64_t TRB_TYPE_ENABLE_SLOT = 9;
        static constexpr uint64_t TRB_TYPE_ADDRESS_DEVICE = 11;
        static constexpr uint64_t TRB_TYPE_CONFIGURE_EP = 12;
        static constexpr uint64_t TRB_CMD_NOOP = 23;
        static constexpr uint64_t TRB_TRANSFER_EVENT = 32;
        static constexpr uint64_t TRB_TYPE_CMD_COMPLETE = 33;
        static constexpr uint64_t TRB_TYPE_PORT_STATUS = 34;

        static constexpr uint64_t XHCI_PORT_REG_SET_OFFSET = 0x400;
        static constexpr uint64_t XHCI_PORT_REG_STRIDE = 0x10;

        CapabilityRegisters *cap_regs;
        OperationalRegisters *op_regs;
        RuntimeRegisters *run_regs;
        DoorbellRegister *db_regs;
        
        EventRingSegmentTableEntry *erst_virt;
        TRB *cmd_ring_base;

        uint64_t *dcbaap_virt;
        uint64_t *cmd_ring_virt;
        uint64_t *event_ring_virt;

        // [slot_id][dci]
        uint64_t ***ep_rings;
        uint8_t **ep_cycle_states;
        uint64_t **ep_enqueue_ptrs;
        uint64_t **ep_ring_physs;

        void *descriptor_buffer = nullptr;

        // USBDriver **attached_drivers;

        enum class SetupState {
            STATE_NONE,
            STATE_ADDRESSING,
            STATE_GET_DEV_DESC,
            STATE_GET_CONFIG_DESC_HEADER,
            STATE_GET_CONFIG_DESC_FULL,
            STATE_CONFIGURING_EP,
            STATE_SETTING_CONFIG,
            STATE_CONFIGURED
        };

        SetupState *slot_states;
        uint8_t *config_descriptor_buffer = nullptr;
        uint8_t *pending_config_value;

        void reset_controller();
    };
}