# arch/x86/kernel/module.mk
exe-y        := arch/x86/kernel
exe-build-y  := $(BUILD_DIR)/kernel
exe-output-y := $(BUILD_DIR)/kernel.elf
exe-ldflags-y := -nostdlib -T $(ROOT_DIR)/arch/x86/kernel/kernel.ld
exe-libs-y   := $(LIBS)
exe-objs-y   := $(KCOMMON_OBJS)
