#pragma once
#include <stdint.h>
#include <HAL/PCI/xHCI/xHCIDriver.hpp>

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
        bool poll_event_ring();
        void check_ports();
        void reset_ports(int port_index);
        void bios_handoff();
        void addr_device(uint8_t slot_id, int root_port_index);

        void send_control_request(uint8_t slot_id, uint8_t requestType, uint8_t request, uint16_t value, uint16_t index, uint16_t length, uint64_t buffer_phys);
        void conf_endpoint(uint8_t slot_id, uint8_t endpoint_address, uint8_t endpoint_type, uint16_t max_packet_size);
        void queue_int_transfer(uint8_t slot_id, uint8_t endpoint_index, uint64_t buffer_phys, uint32_t buffer_size);
        void queue_bulk_transfer(uint8_t slot_id, uint8_t endpoint_address, uint64_t buffer_phys, uint32_t buffer_size);
        void conf_interface_endpoints(uint8_t slot_id, ParsedEndpoint* endpoints, int ep_count);
        static uint8_t GetPortSpeed(uint32_t portsc);

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

        enum : uint32_t {
            CAP_ID_USB_LEGACY = 1,

            HCC_PARAMS_XECP_SHIFT = 16,
            HCC_PARAMS_XECP_MASK = 0xFFFF,

            USBLEGSUP_BIOS_OWNED = (1 << 16),
            USBLEGSUP_OS_OWNED = (1 << 24),

            CAP_NEXT_SHIFT = 8,
            CAP_NEXT_MASK = 0xFF,
            CAP_ID_MASK = 0xFF
        };

        enum class xHCISpeedID : uint8_t {
            FullSpeed       = 1,
            LowSpeed        = 2,
            HighSpeed       = 3,
            SuperSpeed      = 4,
            SuperSpeedPlus  = 5
        };

        enum class xHCIEpType : uint32_t {
            NotValid = 0,
            IsochOut = 1,
            BulkOut = 2,
            InterruptOut = 3,
            Control = 4,
            IsochIn = 5,
            BulkIn = 6,
            InterruptIn = 7
        };

        enum class xHCITransferType : uint32_t {
            NoDataStage  = 0,
            OutDataStage = 2,
            InDataStage  = 3
        };

        enum class xHCITrbType : uint32_t {
            Normal       = 1,
            SetupStage   = 2,
            DataStage    = 3,
            StatusStage  = 4,
            Link         = 6,
            CommandNoOp  = 23
        };

        static constexpr uint64_t TRB_TYPE_ENABLE_SLOT = 9;
        static constexpr uint64_t TRB_TYPE_ADDRESS_DEVICE = 11;
        static constexpr uint64_t TRB_TYPE_CONFIGURE_EP = 12;
        static constexpr uint64_t TRB_CMD_NOOP = 23;
        static constexpr uint64_t TRB_TRANSFER_EVENT = 32;
        static constexpr uint64_t TRB_TYPE_CMD_COMPLETE = 33;
        static constexpr uint64_t TRB_TYPE_PORT_STATUS = 34;

        static constexpr uint64_t XHCI_PORT_REG_SET_OFFSET = 0x400;
        static constexpr uint64_t XHCI_PORT_REG_STRIDE = 0x10;

        static constexpr uint32_t XHCI_PORTSC_SPEED_SHIFT = 10;
        static constexpr uint32_t XHCI_PORTSC_SPEED_MASK = 0xF;

        static constexpr uint16_t USB_EP0_MAX_PACKET_SUPERSPEED = 512;
        static constexpr uint16_t USB_EP0_MAX_PACKET_HIGHSPEED = 64;
        static constexpr uint16_t USB_EP0_MAX_PACKET_DEFAULT = 8;

        static constexpr uint8_t USB_REQUEST_DIR_IN = 0x80;

        static constexpr uint32_t XHCI_INPUT_ADD_SLOT_CONTEXT = (1 << 0);
        static constexpr uint32_t XHCI_INPUT_ADD_EP0_CONTEXT = (1 << 1);

        static constexpr uint32_t XHCI_SLOT_CONTEXT_ENTRIES_SHIFT = 27;
        static constexpr uint32_t XHCI_SLOT_SPEED_SHIFT = 20;

        static constexpr uint32_t XHCI_SLOT_ROOT_PORT_SHIFT = 16;
        static constexpr uint8_t  XHCI_EP_TYPE_OUT_SHIFT = 0;
        static constexpr uint8_t  XHCI_EP_TYPE_IN_SHIFT = 4;

        static constexpr uint32_t XHCI_EP_CERR_SHIFT = 1;
        static constexpr uint32_t XHCI_EP_CERR_MAX = 3;
        static constexpr uint32_t XHCI_EP_TYPE_SHIFT = 3;
        static constexpr uint32_t XHCI_EP_MAX_PACKET_SHIFT = 16;
        static constexpr uint32_t XHCI_EP_MAX_ERROR_COUNT = 3;
        static constexpr uint32_t XHCI_EP_INDEX_0 = 1;
        static constexpr uint32_t XHCI_EP_DEFAULT_TRB_LEN = 8;
        static constexpr uint32_t XHCI_CTX_EP0_INDEX = 0;

        static constexpr uint32_t XHCI_PORTSC_CCS = (1U << 0);
        static constexpr uint32_t XHCI_PORTSC_PED = (1U << 1);
        static constexpr int XHCI_NO_PENDING_PORT = -1;
        static constexpr uint32_t XHCI_PORTSC_PR  = (1U << 4);
        static constexpr uint32_t XHCI_PORTSC_RW1C_MASK = 
            (1U << 17) |
            (1U << 18) |
            (1U << 19) |
            (1U << 20) |
            (1U << 21) |
            (1U << 22) |
            (1U << 23);

        static constexpr uint32_t XHCI_TRB_CYCLE_SHIFT = 0;
        static constexpr uint32_t XHCI_TRB_IDT_SHIFT = 6;
        static constexpr uint32_t XHCI_TRB_TYPE_SHIFT = 10;
        static constexpr uint32_t XHCI_TRB_TRT_SHIFT = 16;
        static constexpr uint32_t XHCI_TRB_DIR_SHIFT = 16;
        static constexpr uint32_t XHCI_TRB_SLOT_ID_SHIFT = 24;
        static constexpr uint32_t XHCI_TRB_DATA_DIR_OUT = 0;
        static constexpr uint32_t XHCI_TRB_DATA_DIR_IN  = 1;
        static constexpr uint32_t TRB_TRANSFER_LEN_MASK = 0xFFFFFF;

        static constexpr uint32_t XHCI_COMP_SUCCESS = 1;
        static constexpr uint32_t XHCI_COMP_SHORT_PACKET = 13;

        static constexpr uint8_t DESC_TYPE_INTERFACE = 0x04;
        static constexpr uint8_t DESC_TYPE_ENDPOINT = 0x05;

        static constexpr uint16_t CONFIG_HEADER_SIZE = 9;
        static constexpr uint16_t MAX_PARSED_ENDPOINTS = 16;

        static constexpr uint32_t XHCI_TRB_IOC_SHIFT = 5;
        static constexpr uint32_t XHCI_TRB_STATUS_DIR_SHIFT = 16;

        static constexpr uint32_t XHCI_TRB_STATUS_DIR_OUT = 0;
        static constexpr uint32_t XHCI_TRB_STATUS_DIR_IN = 1;
        static constexpr uint32_t XHCI_DB_TARGET_EP0 = 1;
        
        static constexpr uint32_t USB_SETUP_PACKET_SIZE = 8;
        static constexpr uint32_t USB_TOTAL_xHCI_SLOTS = 256;
        static constexpr uint32_t XHCI_CMD_RING_INDEX_LIMIT = USB_TOTAL_xHCI_SLOTS - 1;
        static constexpr uint32_t XHCI_HC_DOORBELL = 0;
        static constexpr uint32_t XHCI_DB_TARGET_HC = 0;
        static constexpr uint32_t XHCI_EVENT_RING_TRBS = 512;
        static constexpr uint32_t DEVICE_CONTEXT_ENTRIES = 32;

        static constexpr uint64_t XHCI_DEQ_CYCLE_STATE_START = (1ULL << 0);
        static constexpr uint8_t BAR_INDEX_0 = 0;
        static constexpr uint8_t MMIO_MAP_PAGES = 16;
        static constexpr uint32_t XHCI_TRB_ISP_ENABLE = (1U << 2);

        static constexpr uint64_t XHCI_CAP_HCSPARAMS1 = 0x04;
        static constexpr uint32_t XHCI_HCSPARAMS1_MAX_PORTS_SHIFT = 24;
        static constexpr uint32_t XHCI_HCSPARAMS1_MAX_PORTS_MASK = 0xFF;
        static constexpr uint64_t XHCI_CRCR_RCS = (1ULL << 0);
        static constexpr uint8_t XHCI_ERST_PRIMARY_SEGMENT = 0;
        static constexpr uint32_t XHCI_PRIMARY_INTERRUPTER = 0;
        static constexpr uint32_t XHCI_ERST_SINGLE_SEGMENT = 1;
        static constexpr uint64_t XHCI_ERDP_EHB = (1ULL << 3);

        static constexpr uint32_t XHCI_IMAN_IP = (1U << 0);
        static constexpr uint32_t XHCI_IMAN_IE = (1U << 1);
        static constexpr uint32_t XHCI_IMOD_1MS_INTERVAL = 4000;

        static constexpr uint32_t XHCI_CMD_RUN_STOP = (1U << 0);
        static constexpr uint32_t XHCI_CMD_HCRST = (1U << 1);
        static constexpr uint32_t XHCI_CMD_INTE = (1U << 2);
        static constexpr uint32_t XHCI_CMD_HSEE = (1U << 3);

        static constexpr uint32_t XHCI_STS_HCHALTED = (1U << 0);
        static constexpr uint32_t XHCI_STS_CNR = (1U << 11);
        static constexpr uint8_t XHCI_COMMAND_RING_CYCLE_START = 1;

        static constexpr uint32_t TRB_TYPE_NORMAL = 1;
        static constexpr uint32_t TRB_TYPE_LINK = 6;
        static constexpr uint32_t XHCI_TRB_LINK_TC = (1U << 1);
        static constexpr uint16_t XHCI_MAX_EVENTS_PER_INTR = 16;

        static constexpr uint32_t XHCI_TRB_CYCLE_MASK = 0x1;
        static constexpr uint32_t XHCI_TRB_TYPE_MASK = 0x3F;
        static constexpr uint32_t XHCI_TRB_SLOT_ID_MASK = 0xFF;
        static constexpr uint32_t XHCI_TRB_SIZE_BYTES = 16;

        static constexpr uint32_t XHCI_TRB_COMP_CODE_SHIFT = 24;
        static constexpr uint32_t XHCI_TRB_COMP_CODE_MASK = 0xFF;

        static constexpr uint16_t XHCI_CODE_SUCCESS = 1;
        static constexpr int16_t PENDING_PORT_COMPLETE = -1;

        static constexpr uint32_t XHCI_TRB_IOC_ENABLE = (1U << 5);
        static constexpr uint64_t ENDPOINT_ID_BITMASK = 0x1F0000;
        static constexpr uint8_t ENDPOINT_ID_SHIFT = 16;

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

        void **descriptor_buffer = nullptr;

        xHCIDriver **attached_drivers;

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

        volatile SetupState *slot_states;
        uint8_t **config_descriptor_buffer = nullptr;
        uint8_t *pending_config_value;

        void reset_controller();
    };

    namespace USB {
        constexpr uint32_t DEVICE_DESCRIPTOR_SIZE = 18;

        constexpr uint8_t REQ_DIR_IN = 0x80;
        constexpr uint8_t REQ_DIR_OUT = 0x0;
        constexpr uint8_t REQ_SET_CONFIGURATION = 0x09;
        constexpr uint8_t REQ_TYPE_STANDARD = 0x00;
        constexpr uint8_t REQ_REC_DEVICE = 0x00;

        constexpr uint8_t REQ_GET_DESCRIPTOR = 0x06;

        constexpr uint16_t DESC_TYPE_DEVICE = 0x01;
        constexpr uint16_t DESC_TYPE_CONFIG = 0x02;
        constexpr uint16_t DESC_TYPE_STRING = 0x03;
        constexpr uint16_t DESC_TYPE_INTERFACE = 0x04;
        constexpr uint16_t DESC_TYPE_ENDPOINT = 0x05;
        constexpr uint16_t DEFAULT_CONFIGURATION_VALUE = 1;

        static constexpr uint8_t  USB_EP_NUM_MASK = 0x0F;
        static constexpr uint8_t  USB_EP_DIR_IN = 0x80;
        static constexpr uint32_t USB_EP_TYPE_MASK = 0x03;

        constexpr uint16_t make_wValue(uint8_t type, uint8_t index) {
            return (static_cast<uint16_t>(type) << 8) | index;
        }
    }
}