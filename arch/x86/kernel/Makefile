CROSS = x86_64-elf-

# Tools
AS = $(CROSS)as
CC = $(CROSS)gcc
LD = $(CROSS)ld
OBJCOPY = $(CROSS)objcopy
# HOST_GCC = gcc
# HOST_LD = ld
HOST_OBJCOPY = objcopy

# Flags
CFLAGS = -ffreestanding -m64 -O2 -Wall -Wextra -c
LDFLAGS = -nostdlib -T kernel/linker.ld -lefi -lgnuefi
OBJCPYFLAGS = binary

# LIBS := -lefi -lgnuefi

INCLUDES := -Iinclude -Ignu-efi/inc

# Directories
SRC_DIRS := kernel
BUILD_DIR ?= build

# Output ELF and ISO
OUTPUT := $(BUILD_DIR)/kernel.elf
ISO := iso/kernel.bin

# Kernel object files
SRC_FILES := $(shell find $(SRC_DIRS) -type f -name "*.c")
OBJ_FILES := $(patsubst %.c,$(BUILD_DIR)/%.o,$(SRC_FILES))

KERNEL_OBJ := $(BUILD_DIR)/kernel/kernel.o
LINK_OBJS := $(KERNEL_OBJ) $(filter-out $(KERNEL_OBJ),$(OBJ_FILES))

# UEFI bootloader
BOOT_SRC := boot
BOOT_SRCS := $(shell find $(BOOT_SRC) -type f -name "*.c")
BOOT_OBJ := $(patsubst %.c,$(BUILD_DIR)/%.o,$(BOOT_SRCS))
BOOT_SO  := $(BUILD_DIR)/boot/boot.so
EFI_OUT  := iso/EFI/BOOT/BOOTx64.EFI

BOOT_CFLAGS := -Iinclude -Ignu-efi/inc -fpic -ffreestanding -fno-stack-protector -fno-stack-check -fshort-wchar -mno-red-zone -maccumulate-outgoing-args -c
BOOT_LDFLAGS := -shared -Bsymbolic -Lgnu-efi/x86_64/lib -Lgnu-efi/x86_64/gnuefi -Tgnu-efi/gnuefi/elf_x86_64_efi.lds
BOOT_LIBS := -lgnuefi -lefi
EFI_SECTIONS := -j .text -j .sdata -j .data -j .rodata -j .dynamic -j .dynsym -j .rel -j .rela -j .rel.* -j .rela.* -j .reloc

.PHONY: all run build clean bootloader app

# Default target
all: run

run: $(OUTPUT) $(ISO)
	@echo "Build successful."

# Kernel object files
build: $(OBJ_FILES)

$(BUILD_DIR)/%.o: %.c
	@echo "Compiling C: $< -> $@"
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INCLUDES) $< -o $@

# Kernel linking
$(OUTPUT): $(OBJ_FILES)
	@echo "Linking kernel ELF: $@"
	$(LD) $(LDFLAGS) -o $@ $(LINK_OBJS)

# Kernel binary
$(ISO): $(OUTPUT)
	$(OBJCOPY) -O $(OBJCPYFLAGS) $< $@

# Bootloader
bootloader: $(EFI_OUT)

# Compile all boot .c -> .o
$(BUILD_DIR)/boot/%.o: boot/%.c
	@echo "Compiling bootloader: $< -> $@"
	@mkdir -p $(dir $@)
	$(HOST_GCC) $(BOOT_CFLAGS) $< -o $@

# Link bootloader SO from all boot .o files
$(BOOT_SO): $(BOOT_OBJ)
	@echo "Linking bootloader SO: $@"
	@mkdir -p $(dir $@)
	$(HOST_LD) $(BOOT_LDFLAGS) gnu-efi/x86_64/gnuefi/crt0-efi-x86_64.o $^ -o $@ $(BOOT_LIBS)

# Create EFI binary
$(EFI_OUT): $(BOOT_SO)
	@mkdir -p $(dir $@)
	$(HOST_OBJCOPY) $(EFI_SECTIONS) --target=efi-app-x86_64 --subsystem=10 $< $@

# Clean
clean:
	@echo "Cleaning..."
	@rm -rf $(BUILD_DIR) $(EFI_OUT)


