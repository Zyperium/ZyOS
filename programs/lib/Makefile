TARGET      := ../build/libzyos.a

SRC_DIR     := src
INC_DIR     := include
BUILD_DIR   := ../build/lib

CC          := x86_64-elf-gcc
ASM         := nasm
AR          := x86_64-elf-ar
ARFLAGS     := rcs

CFLAGS      := -Wall -Wextra -O0 -ffreestanding -nostdlib -I$(INC_DIR)
ASMFLAGS    := -f elf64 -I$(INC_DIR)/

C_SRCS      := $(shell find $(SRC_DIR) -type f -name '*.c')
ASM_SRCS    := $(shell find $(SRC_DIR) -type f \( -name '*.asm' -o -name '*.s' \))

C_OBJS      := $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(C_SRCS))
ASM_OBJS    := $(patsubst $(SRC_DIR)/%.asm, $(BUILD_DIR)/%.o, $(ASM_SRCS:.s=.o))

OBJS        := $(C_OBJS) $(ASM_OBJS)

DEPS        := $(C_OBJS:.o=.d)

.PHONY: all
all: $(TARGET)

$(TARGET): $(OBJS)
	@echo "  AR      $@"
	@$(AR) $(ARFLAGS) $@ $^

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.asm
	@mkdir -p $(dir $@)
	@echo "  ASM     $<"
	@$(ASM) $(ASMFLAGS) $< -o $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.s
	@mkdir -p $(dir $@)
	@echo "  ASM     $<"
	@$(ASM) $(ASMFLAGS) $< -o $@

-include $(DEPS)

.PHONY: clean
clean:
	@echo "  CLEAN"
	@rm -rf $(BUILD_DIR) $(TARGET)