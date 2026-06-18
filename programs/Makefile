# ZyOS program makefile
PROGRAM_DIRS := $(sort $(dir $(wildcard */Makefile)))
GLOBAL_INCLUDE_DIR := $(abspath ./cstdlib)

CFLAGS   += -I$(GLOBAL_INCLUDE_DIR)
CXXFLAGS += -I$(GLOBAL_INCLUDE_DIR)

export CFLAGS
export CXXFLAGS

.PHONY: all clean $(PROGRAM_DIRS)

all:
ifeq ($(PROGRAM_DIRS),)
	@echo " No programs found "
else
	@echo " Dispatching Program Builds... "
	@$(MAKE) $(PROGRAM_DIRS)
endif

$(PROGRAM_DIRS):
	@echo "[Module Target] Entering: $@"
	@$(MAKE) -C $@

clean:
ifeq ($(PROGRAM_DIRS),)
	@echo "No programs to clean."
else
	@for dir in $(PROGRAM_DIRS); do \
		echo "[Module Clean] Entering: $$dir"; \
		$(MAKE) -C $$dir clean; \
	done
endif