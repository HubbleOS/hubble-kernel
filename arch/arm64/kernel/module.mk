ifeq ($(ARCH),arm64)
    exe-y        := arch/arm64/kernel
    exe-output-y := $(BUILD_DIR)/kernel.elf
    exe-ldflags-y := -nostdlib -T $(ROOT_DIR)/arch/arm64/kernel/kernel.ld
    exe-objs-y   :=  
    exe-libs-y   :=  
endif
