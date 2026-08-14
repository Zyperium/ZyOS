TARGET      := ../build/winlib.a

SRC_DIR     := src
INC_DIR     := include
BUILD_DIR   := ../build/winlib

CC          := x86_64-elf-gcc
CXX         := x86_64-elf-g++
ASM         := nasm
AR          := x86_64-elf-ar
ARFLAGS     := rcs

COMMON_FLAGS := -O3 -m64 -ffreestanding -fno-pie -mcmodel=small \
                -march=x86-64-v3 \
                -Wall -Wextra -Iinclude -I../lib/include -mstackrealign
CFLAGS       := $(COMMON_FLAGS)
CXXFLAGS     := $(COMMON_FLAGS) -fno-exceptions -fno-rtti -std=c++20
ASMFLAGS     := -f elf64 -I$(INC_DIR)/

C_SRCS      := $(shell find $(SRC_DIR) -type f -name '*.c')
CPP_SRCS    := $(shell find $(SRC_DIR) -type f -name '*.cpp')
CC_SRCS     := $(shell find $(SRC_DIR) -type f -name '*.cc')
CXX_SRCS    := $(shell find $(SRC_DIR) -type f -name '*.cxx')
ASM_SRCS    := $(shell find $(SRC_DIR) -type f -name '*.asm')
S_SRCS      := $(shell find $(SRC_DIR) -type f -name '*.s')

C_OBJS      := $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%_c.o, $(C_SRCS))
CPP_OBJS    := $(patsubst $(SRC_DIR)/%.cpp, $(BUILD_DIR)/%_cpp.o, $(CPP_SRCS))
CC_OBJS     := $(patsubst $(SRC_DIR)/%.cc, $(BUILD_DIR)/%_cc.o, $(CC_SRCS))
CXX_OBJS    := $(patsubst $(SRC_DIR)/%.cxx, $(BUILD_DIR)/%_cxx.o, $(CXX_SRCS))
ASM_OBJS    := $(patsubst $(SRC_DIR)/%.asm, $(BUILD_DIR)/%_asm.o, $(ASM_SRCS))
S_OBJS      := $(patsubst $(SRC_DIR)/%.s, $(BUILD_DIR)/%_s.o, $(S_SRCS))

OBJS        := $(C_OBJS) $(CPP_OBJS) $(CC_OBJS) $(CXX_OBJS) $(ASM_OBJS) $(S_OBJS)
DEPS        := $(OBJS:.o=.d)

.PHONY: all
all: $(TARGET)

$(TARGET): $(OBJS)
	@echo "  AR      $@"
	@$(AR) $(ARFLAGS) $@ $^

$(BUILD_DIR)/%_c.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/%_cpp.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(dir $@)
	@echo "  CXX     $<"
	@$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/%_cc.o: $(SRC_DIR)/%.cc
	@mkdir -p $(dir $@)
	@echo "  CXX     $<"
	@$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/%_cxx.o: $(SRC_DIR)/%.cxx
	@mkdir -p $(dir $@)
	@echo "  CXX     $<"
	@$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/%_asm.o: $(SRC_DIR)/%.asm
	@mkdir -p $(dir $@)
	@echo "  ASM     $<"
	@$(ASM) $(ASMFLAGS) $< -o $@

$(BUILD_DIR)/%_s.o: $(SRC_DIR)/%.s
	@mkdir -p $(dir $@)
	@echo "  ASM     $<"
	@$(ASM) $(ASMFLAGS) $< -o $@

-include $(DEPS)

.PHONY: clean
clean:
	@echo "  CLEAN"
	@rm -rf $(BUILD_DIR) $(TARGET)