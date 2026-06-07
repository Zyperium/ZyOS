# ZyOS driver makefile
DRIVER_DIRS := $(sort $(dir $(wildcard */Makefile)))

.PHONY: all clean $(DRIVER_DIRS)

all:
ifeq ($(DRIVER_DIRS),)
	@echo " No drivers found "
else
	@echo " Dispatching Driver Builds... "
	@$(MAKE) $(DRIVER_DIRS)
endif

$(DRIVER_DIRS):
	@echo "[Module Target] Entering: $@"
	@$(MAKE) -C $@

clean:
ifeq ($(DRIVER_DIRS),)
	@echo "No drivers to clean."
else
	@for dir in $(DRIVER_DIRS); do \
		echo "[Module Clean] Entering: $$dir"; \
		$(MAKE) -C $$dir clean; \
	done
endif