# arch/x86/kernel/Makefile
KERNEL_DIR := .

# Output paths
KERNEL_BUILD_DIR := $(BUILD_DIR)/kernel

OUTPUT := $(BUILD_DIR)/kernel.elf
ISO := $(ISO_DIR)/kernel.bin

.PHONY: all build iso

# Default target
all: build iso

KCOMMON_OBJS := $(BUILD_DIR)/kernel_common/main.o
KCOMMON_OBJS += $(BUILD_DIR)/kernel_common/init/fs.o
KCOMMON_OBJS += $(BUILD_DIR)/kernel_common/printk.o

build: $(BUILD_TOOL)
	$(Q)$(BUILD_TOOL) $(BUILD_TOOL_FLAGS) \
		--src-dir $(KERNEL_DIR) \
		--build-dir $(KERNEL_BUILD_DIR) \
		--cc $(CC) \
		--asm $(ASM) \
		--cflags "$(CFLAGS)" \
		--asmflags "$(ASMFLAGS)" \
		--includes "$(INCLUDES)" \
		--output $(OUTPUT) \
		--type exe \
		--ld $(LD) \
		--ldflags "-nostdlib -T $(KERNEL_DIR)/kernel.ld" \
		--obj-files "$(KCOMMON_OBJS)" \
		--libs "$(NET_LIB) $(FS_LIB) $(DRIVERS_LIB) $(FONT_LIB) $(COLOR_LIB) $(STRING_LIB) $(CTYPE_LIB)" 

# kernel.elf to kernel.bin for ISO
iso: $(ISO)
$(ISO): $(OUTPUT)
	@mkdir -p $(dir $@)
	$(OBJCOPY) -O binary $< $@
