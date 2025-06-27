# arch/x86/Makefile
# Directories
include $(CONFIG_MK)

SRC_DIR := src
BUILD_DIR ?= build

# Output ELF and ISO
OUTPUT := $(BUILD_DIR)/kernel.elf
ISO_DIR ?= iso
ISO := $(ISO_DIR)/kernel.bin

# Kernel object files
SRC_FILES := $(shell find $(SRC_DIR) -type f -name "*.c")
OBJ_FILES := $(patsubst %.c,$(BUILD_DIR)/%.o,$(SRC_FILES))

CPP_SRC_FILES := $(shell find apps -type f -name "*.cpp")
CPP_OBJ_FILES := $(patsubst %.cpp,$(BUILD_DIR)/%.o,$(CPP_SRC_FILES))

OBJ_FILES += $(CPP_OBJ_FILES)

KERNEL_OBJ := $(BUILD_DIR)/$(SRC_DIR)/kmain.o
LINK_OBJS := $(KERNEL_OBJ) $(filter-out $(KERNEL_OBJ),$(OBJ_FILES))

# UEFI bootloader
BOOT_SRC := boot
BOOT_SRCS := $(shell find $(BOOT_SRC) -type f -name "*.c")
BOOT_OBJ := $(patsubst %.c,$(BUILD_DIR)/%.o,$(BOOT_SRCS))
BOOT_SO  := $(BUILD_DIR)/boot/boot.so
EFI_OUT  := $(ISO_DIR)/EFI/BOOT/BOOTx64.EFI
EFI_OUT_DIR := $(ISO_DIR)/EFI/BOOT

INCLUDES += include

.PHONY: all bootloader libc build

# Default target
all: build $(ISO) bootloader

build: libc $(OBJ_FILES)

# Kernel object files
build: $(OBJ_FILES)

$(BUILD_DIR)/%.o: %.c
	@echo "Compiling C: $< -> $@"
	@mkdir -p $(dir $@)
	$(CC) -g $(CFLAGS) $(INCLUDES) $< -o $@

$(BUILD_DIR)/%.o: %.cpp
	@echo "Compiling C++: $< -> $@"
	@mkdir -p $(dir $@)
	$(CXX) -g $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Kernel linking
# $(OUTPUT): $(OBJ_FILES) $(LIBC_A)
# 	@echo "Linking kernel ELF: $@"
# 	$(LD) $(LDFLAGS) -o $@ $(LINK_OBJS) $(LIBC_A)

$(OUTPUT): $(OBJ_FILES)
	@echo "LINK_OBJS: $(LINK_OBJS)"
	@echo "Linking kernel ELF (via g++): $@"
	$(CXX) $(LDFLAGS) $(SRC_DIR)/linker.ld -o $@ $(LINK_OBJS) $(BUILD_DIR)/libc.a

# Kernel binary
$(ISO): $(OUTPUT)
	@mkdir -p $(dir $@)
	$(OBJCOPY) -O $(OBJCPYFLAGS) $< $@

# Bootloader
bootloader: $(EFI_OUT)

# Compile all boot .c -> .o
$(BUILD_DIR)/boot/%.o: boot/%.c
	@echo "Compiling bootloader: $< -> $@"
	@mkdir -p $(dir $@)
	$(CC) $(BOOT_CFLAGS) $< -o $@

# Link bootloader SO from all boot .o files
$(BOOT_SO): $(BOOT_OBJ)
	@echo "Linking bootloader SO: $@"
	@mkdir -p $(dir $@)
	$(LD) $(BOOT_LDFLAGS) gnu-efi/x86_64/gnuefi/crt0-efi-x86_64.o $^ -o $@ $(BOOT_LIBS)

# Create EFI binary
$(EFI_OUT): $(BOOT_SO)
	@mkdir -p $(EFI_OUT_DIR)
	$(HOST_OBJCOPY) $(EFI_SECTIONS) --target=efi-app-x86_64 --subsystem=10 $< $@
