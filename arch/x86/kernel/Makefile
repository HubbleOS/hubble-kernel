# arch/x86/kernel/Makefile
KERNEL_DIR := .

# Output paths
KERNEL_BUILD_DIR := $(BUILD_DIR)/kernel

OUTPUT := $(BUILD_DIR)/kernel.elf
ISO := $(ISO_DIR)/kernel.bin

.PHONY: all build iso

# Default target
all: build iso

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
		--libs "$(NET_LIB) $(FS_LIB) $(DRIVERS_LIB)" 

# kernel.elf to kernel.bin for ISO
iso: $(ISO)
$(ISO): $(OUTPUT)
	@mkdir -p $(dir $@)
	$(OBJCOPY) -O binary $< $@
