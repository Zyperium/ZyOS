#pragma once
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PCI_ANY_ID (~0)

enum {
	PCI_ID_F_VFIO_DRIVER_OVERRIDE = 1,
};

struct pci_device_id {
	uint32_t vendor, device;
	uint32_t subvendor, subdevice;
	uint32_t _class, class_mask; // This is defined as "class" in the linux kernel. Thanks C++!
	uint64_t driver_data;
	uint32_t override_only;
};

#define IEEE1394_MATCH_VENDOR_ID	0x0001
#define IEEE1394_MATCH_MODEL_ID		0x0002
#define IEEE1394_MATCH_SPECIFIER_ID	0x0004
#define IEEE1394_MATCH_VERSION		0x0008

struct ieee1394_device_id {
	uint32_t match_flags;
	uint32_t vendor_id;
	uint32_t model_id;
	uint32_t specifier_id;
	uint32_t version;
	uint64_t driver_data;
};

#ifdef __cplusplus
}
#endif