MODULE_NAME ?= 8139too.ko
OUTPUT_DIR  ?= ../build
SRC_DIR     := .

CC     := x86_64-elf-g++
CC_C   := x86_64-elf-gcc
LD     := x86_64-elf-ld
ASM    := nasm

MODULE_NAME_BASE := $(basename $(MODULE_NAME))
OBJ_DIR          := $(OUTPUT_DIR)/$(MODULE_NAME_BASE)

CPPSRC := $(shell find $(SRC_DIR) -name '*.cpp')
CSRC   := $(shell find $(SRC_DIR) -name '*.c')
ASMSRC := $(shell find $(SRC_DIR) -name '*.asm')

OBJS := $(CPPSRC:%.cpp=$(OBJ_DIR)/%.o) \
        $(CSRC:%.c=$(OBJ_DIR)/%.o)     \
        $(ASMSRC:%.asm=$(OBJ_DIR)/%.o)

COMMON_FLAGS := -O2 -m64 -ffreestanding -fno-pie -mcmodel=large \
                -Wall -Wextra

CFLAGS   += $(COMMON_FLAGS) -std=c11
CXXFLAGS += $(COMMON_FLAGS) -std=c++20 -fno-rtti -fno-exceptions

ASMFLAGS := -f elf64
LDFLAGS  := -r -m elf_x86_64

.PHONY: all clean

all: $(OUTPUT_DIR)/$(MODULE_NAME)

$(OUTPUT_DIR)/$(MODULE_NAME): $(OBJS)
	@mkdir -p $(@D)
	@echo "[LD] $@"
	@$(LD) $(LDFLAGS) -o $@ $(OBJS)

$(OBJ_DIR)/%.o: %.cpp
	@mkdir -p $(@D)
	@echo "[CXX] $<"
	@$(CC) $(CXXFLAGS) -c $< -o $@

$(OBJ_DIR)/%.o: %.c
	@mkdir -p $(@D)
	@echo "[CC] $<"
	@$(CC_C) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/%.o: %.asm
	@mkdir -p $(@D)
	@echo "[ASM] $<"
	@$(ASM) $(ASMFLAGS) $< -o $@

clean:
	@rm -rf $(OUTPUT_DIR)/$(MODULE_NAME) $(OBJ_DIR)