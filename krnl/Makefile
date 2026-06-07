# ZyOS core kernel

KERNEL_NAME ?= zyoskrnl.elf
BUILD_DIR   ?= build
SRC_DIR     := .
LIMINE_DIR  := ../limine

CC      := x86_64-elf-g++
CC_C    := x86_64-elf-gcc
LD      := x86_64-elf-ld
ASM     := nasm

KERNEL_CFLAGS := -g -O2 -ffreestanding -mcmodel=kernel -mno-red-zone \
                 -fno-exceptions -fno-rtti -Wall -Wextra -I$(LIMINE_DIR) -I$(SRC_DIR) \
                 -mgeneral-regs-only -std=c++23 -fno-threadsafe-statics \
                 -fno-pic -fno-omit-frame-pointer -fstack-protector-all

KERNEL_CFLAGS_C := -g -O2 -ffreestanding -mcmodel=kernel -mno-red-zone \
                   -Wall -Wextra -I$(LIMINE_DIR) -I$(SRC_DIR) \
                   -mgeneral-regs-only -std=gnu11 -fno-pic \
                   -fno-omit-frame-pointer

ASMFLAGS := -f elf64
LDFLAGS  := -T $(SRC_DIR)/linker.ld -z max-page-size=0x1000 -static

KERNEL_CPP_SRCS := $(shell find $(SRC_DIR) -name "*.cpp" 2>/dev/null)
KERNEL_C_SRCS   := $(shell find $(SRC_DIR) -name "*.c" 2>/dev/null)
KERNEL_ASM_SRCS := $(shell find $(SRC_DIR) -name "*.asm" 2>/dev/null)

KERNEL_CPP_OBJS := $(patsubst $(SRC_DIR)/%.cpp, $(BUILD_DIR)/%.o, $(KERNEL_CPP_SRCS))
KERNEL_C_OBJS   := $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(KERNEL_C_SRCS))
KERNEL_ASM_OBJS := $(patsubst $(SRC_DIR)/%.asm, $(BUILD_DIR)/%.o, $(KERNEL_ASM_SRCS))

ALL_KERNEL_OBJS := $(KERNEL_CPP_OBJS) $(KERNEL_C_OBJS) $(KERNEL_ASM_OBJS)

.PHONY: all clean prep

all: prep $(BUILD_DIR)/$(KERNEL_NAME)

prep:
	@mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/$(KERNEL_NAME): $(ALL_KERNEL_OBJS)
	@echo "[LD] Linking Core Kernel: $(KERNEL_NAME)"
	$(LD) $(LDFLAGS) $(ALL_KERNEL_OBJS) -o $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(dir $@)
	@echo "[CXX] $<"
	$(CC) $(KERNEL_CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	@echo "[CC] $<"
	$(CC_C) $(KERNEL_CFLAGS_C) -c $< -o $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.asm
	@mkdir -p $(dir $@)
	@echo "[ASM] $<"
	$(ASM) $(ASMFLAGS) $< -o $@

clean:
	@echo "Cleaning Core Kernel build artifacts..."
	rm -rf $(BUILD_DIR)