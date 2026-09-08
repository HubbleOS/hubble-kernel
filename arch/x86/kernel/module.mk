# arch/x86/kernel/module.mk
obj-y           := arch/x86/boot/limine
exe-y           := arch/x86/kernel
exe-output-y    := $(OUT_DIR)/kernel.elf
exe-ldflags-y   := -nostdlib -T $(ROOT_DIR)/arch/x86/kernel/kernel.ld
exe-ldflags-y   += --whole-archive $(ROOT_DIR)/rust/target/x86_64-unknown-none/release/libhubble_rust.a
exe-whole-archive := drivers/pci drivers/storage sound lib/core
exe-libs        := net fs lib/fonts lib/color
