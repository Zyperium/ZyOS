#pragma once
#define MODULE_AUTHOR(name)
#define MODULE_DESCRIPTION(name)
#define MODULE_LICENSE(name)
#define	MODULE_INFO(tag, info)
#define	MODULE_FIRMWARE(firmware)
#define	MODULE_SUPPORTED_DEVICE(name)
#define	MODULE_IMPORT_NS(_name)

#ifdef __cplusplus
extern "C" {
#endif

#define	MODULE_DEVICE_TABLE(_bus, _table)				\
									\
static device_method_t _ ## _bus ## _ ## _table ## _methods[] = {	\
	DEVMETHOD_END							\
};									\
									\
static driver_t _ ## _bus ## _ ## _table ## _driver = {			\
	"lkpi_" #_bus #_table,						\
	_ ## _bus ## _ ## _table ## _methods,				\
	0								\
};									\
									\
DRIVER_MODULE(lkpi_ ## _table, _bus, _ ## _bus ## _ ## _table ## _driver,\
	0, 0);								\
									\
MODULE_DEVICE_TABLE_BUS_ ## _bus(_bus, _table)

#define module_pci_driver(__pci_driver) \
        int init_module(void) { return pci_register_driver(&__pci_driver); } \
        void cleanup_module(void) { pci_unregister_driver(&__pci_driver); } \

#ifdef KLD_MODULE
#define	THIS_MODULE	((struct module *)&__this_linker_file)
#else
#define	THIS_MODULE	((struct module *)0)
#endif

#define	__MODULE_STRING(x) __stringify(x)

#define	SI_SUB_OFED_PREINIT	(SI_SUB_ROOT_CONF - 2)
#define	SI_SUB_OFED_MODINIT	(SI_SUB_ROOT_CONF - 1)

#ifdef __cplusplus
}
#endif