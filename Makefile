# Makefile

CONFIG_MK := $(abspath tools/config/config.mk)

include $(CONFIG_MK)

INCLUDES := \
	-I $(abspath include) \
	-I $(LIBC_DIR)/include \
	-I $(ARCH_DIR)/include 

.PHONY: all build run clean host-run libc

all: build 

$(OUT_DIR)/$(ARCH)/gnu-efi/.built:
	$(MAKE) -C $(ARCH_DIR)/gnu-efi
	@mkdir -p $(dir $@)
	@touch $@

build: $(OUT_DIR)/$(ARCH)/gnu-efi/.built
	@echo "🛠️  Building kernel for $(ARCH)..."
	$(MAKE) -C $(ARCH_DIR) \
		BUILD_DIR=$(BUILD_DIR) \
		ISO_DIR=$(ISO_DIR) \
		CONFIG_MK=$(CONFIG_MK) \
		LIBS_DIR=$(LIBS_DIR) \
		INCLUDES="$(INCLUDES)" 
	@echo "✅ Build complete for $(ARCH)"

run: build
	@echo "🚀 Running kernel for $(ARCH)..."
	@make host-run

host-run:
	@echo "🖥  Launching QEMU from host..."
	@make $(SCRIPT_DIR) qemu

clean:
	@echo "🧹 Cleaning build output for $(ARCH)..."
	@rm -rf $(OUT_DIR)
	@echo "✅ Clean complete"


include $(SCRIPT_DIR)/scripts.mk
include $(SCRIPT_DIR)/docker.mk
