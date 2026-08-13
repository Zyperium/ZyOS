# ZyOS program makefile
PROGRAM_DIRS       := $(sort $(dir $(wildcard */Makefile)))
GLOBAL_INCLUDE_DIR := $(abspath ./lib/include)
GLOBAL_LINK_DIR    := $(abspath ./linker.ld)
GLOBAL_LIB_FILE    := $(abspath ./build/libzyos.a)

CFLAGS   += -I$(GLOBAL_INCLUDE_DIR)
CXXFLAGS += -I$(GLOBAL_INCLUDE_DIR)

export CFLAGS
export CXXFLAGS
export GLOBAL_LINK_DIR
export GLOBAL_LIB_FILE

LIB_DIR    := lib/
WINLIB_DIR := winlib/

OTHER_DIRS := $(filter-out $(LIB_DIR) $(WINLIB_DIR), $(PROGRAM_DIRS))

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

ifneq ($(filter $(LIB_DIR), $(PROGRAM_DIRS)),)
ifneq ($(filter $(WINLIB_DIR), $(PROGRAM_DIRS)),)
$(WINLIB_DIR): $(LIB_DIR)
endif
endif

ifneq ($(filter $(WINLIB_DIR), $(PROGRAM_DIRS)),)
$(OTHER_DIRS): $(WINLIB_DIR)
else ifneq ($(filter $(LIB_DIR), $(PROGRAM_DIRS)),)
$(OTHER_DIRS): $(LIB_DIR)
endif

clean:
ifeq ($(PROGRAM_DIRS),)
	@echo "No programs to clean."
else
	@for dir in $(PROGRAM_DIRS); do \
		echo "[Module Clean] Entering: $$dir"; \
		$(MAKE) -C $$dir clean; \
	done
endif