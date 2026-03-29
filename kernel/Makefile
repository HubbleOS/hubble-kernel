# kernel/Makefile

KCOMMON_BUILD_DIR := $(BUILD_DIR)/kernel_common

export KCOMMON_BUILD_DIR

build: $(BUILD_TOOL)
	$(Q)$(BUILD_TOOL) $(BUILD_TOOL_FLAGS) \
		--src-dir $(abspath .) \
		--build-dir $(KCOMMON_BUILD_DIR) \
		--cc $(CC) \
		--asm $(ASM) \
		--cflags "$(CFLAGS)" \
		--asmflags "$(ASMFLAGS)" \
		--includes "$(INCLUDES)" \
		--type objects
