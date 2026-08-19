PROGRAM_NAME ?= decoen.zyx
OUTPUT_DIR   ?= ../build
SRC_DIR      := .

CC     := x86_64-elf-g++
CC_C   := x86_64-elf-gcc
LD     := x86_64-elf-ld
ASM    := nasm

PROGRAM_NAME_BASE := $(basename $(PROGRAM_NAME))
OBJ_DIR           := $(OUTPUT_DIR)/$(PROGRAM_NAME_BASE)

CPPSRC := $(shell find $(SRC_DIR) -name '*.cpp')
CSRC   := $(shell find $(SRC_DIR) -name '*.c')
ASMSRC := $(shell find $(SRC_DIR) -name '*.asm')

OBJS := $(CPPSRC:%.cpp=$(OBJ_DIR)/%.o) \
        $(CSRC:%.c=$(OBJ_DIR)/%.o)     \
        $(ASMSRC:%.asm=$(OBJ_DIR)/%.o)

DEPS := $(OBJS:.o=.d)

COMMON_FLAGS := -O3 -m64 -ffreestanding -fno-pie -mcmodel=small \
                -march=x86-64-v3 \
                -Wall -Wextra -Iinclude -mstackrealign

CFLAGS   += $(COMMON_FLAGS) -std=c11
CXXFLAGS += $(COMMON_FLAGS) -std=c++23 -fno-rtti -fno-exceptions -fno-threadsafe-statics -I$(abspath ../winlib/include)

ASMFLAGS := -f elf64

BASE_LDFLAGS := -m elf_x86_64 -z max-page-size=0x1000

ifneq ($(GLOBAL_LINK_DIR),)
    BASE_LDFLAGS += -T $(GLOBAL_LINK_DIR)
endif

.PHONY: all clean

all: $(OUTPUT_DIR)/$(PROGRAM_NAME)

$(OUTPUT_DIR)/$(PROGRAM_NAME): $(OBJS)
	@mkdir -p $(@D)
	@echo "[LD] $@"
	$(LD) $(BASE_LDFLAGS) -o $@ $(OBJS) $(abspath ../build/winlib.a) $(GLOBAL_LIB_FILE)

$(OBJ_DIR)/%.o: %.cpp
	@mkdir -p $(@D)
	@echo "[CXX] $<"
	@$(CC) $(CXXFLAGS) -MMD -MP -c $< -o $@

$(OBJ_DIR)/%.o: %.c
	@mkdir -p $(@D)
	@echo "[CC] $<"
	@$(CC_C) $(CFLAGS) -MMD -MP -c $< -o $@

$(OBJ_DIR)/%.o: %.asm
	@mkdir -p $(@D)
	@echo "[ASM] $<"
	@$(ASM) $(ASMFLAGS) $< -o $@

-include $(DEPS)

clean:
	@rm -rf $(OUTPUT_DIR)/$(PROGRAM_NAME) $(OBJ_DIR)