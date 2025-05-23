CONFIG_MK := $(abspath config/config.mk)

include $(CONFIG_MK)

.PHONY: all build run clean

all: build

build:
	@echo "Building kernel for $(ARCH)..."
	$(MAKE) -C $(ARCH_DIR) \
	BUILD_DIR=$(BUILD_DIR) \
	ISO_DIR=$(ISO_DIR) \
	CONFIG_MK=$(CONFIG_MK)
	@echo "Build complete for $(ARCH)"

run:
	@echo "Running kernel for $(ARCH)..."
	$(MAKE) -C $(ARCH_DIR) run \
	BUILD_DIR=$(abspath $(BUILD_DIR)) \
	ISO_DIR=$(abspath $(ISO_DIR))
	$(MAKE) -C scripts qemu ARCH=$(ARCH)

clean:
	@echo "Cleaning build output for $(ARCH)..."
	@rm -rf $(OUT_DIR)

include scripts/Makefile