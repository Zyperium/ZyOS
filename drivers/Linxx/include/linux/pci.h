#pragma once
#include <linux/pci_ids.h>
#include <linux/mod_devicetable.h>
#include <linux/spinlock.h>

#ifdef __cplusplus
extern "C" {
#endif

struct pci_device_id;
struct pci_dev;
struct pci_driver {
    const char *name;
    const struct pci_device_id *id_table;
    int  (*probe)(struct pci_dev *dev, const struct pci_device_id *id);
    void (*remove)(struct pci_dev *dev);
};

int pci_register_driver(struct pci_driver *driver);
void pci_unregister_driver(struct pci_driver *driver);

#ifdef __cplusplus
}
#endif