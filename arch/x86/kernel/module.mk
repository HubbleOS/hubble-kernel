# arch/x86/kernel/module.mk
exe-y           := arch/x86/kernel
exe-output-y    := $(ISO_DIR)/kernel.elf
exe-ldflags-y   := -nostdlib -T $(ROOT_DIR)/arch/x86/kernel/kernel.ld
exe-whole-archive := drivers sound
exe-libs        := net fs lib/fonts lib/color lib/core
