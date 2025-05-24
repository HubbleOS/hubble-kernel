CONFIG_MK := $(abspath config/config.mk)

include $(CONFIG_MK)

.PHONY: all build run clean

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
		CONFIG_MK=$(CONFIG_MK)
	@echo "✅ Build complete for $(ARCH)"

run: build
	@echo "🚀 Running kernel for $(ARCH)..."
	@make $(SCRIPT_DIR) qemu

clean:
	@echo "🧹 Cleaning build output for $(ARCH)..."
	@rm -rf $(OUT_DIR)
	@echo "✅ Clean complete"

include $(SCRIPT_DIR)/Makefile
