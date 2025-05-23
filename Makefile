# Root Makefile
ARCH ?= x86

OUT_DIR := out
BUILD_DIR := $(OUT_DIR)/${ARCH}/build
ISO_DIR := $(OUT_DIR)/${ARCH}/iso
ARCH_DIR := arch/$(ARCH)

.PHONY: all \
clean \
run \
build \
img \
qemu \
flash \
help

all: build

build:
	@echo "Building kernel..."
	$(MAKE) -C $(ARCH_DIR) \
		BUILD_DIR=$(abspath $(BUILD_DIR)) \
		ISO_DIR=$(abspath $(ISO_DIR))

run:
	@echo "Running kernel..."
	$(MAKE) -C $(ARCH_DIR) run \
		BUILD_DIR=$(abspath $(BUILD_DIR)) \
		ISO_DIR=$(abspath $(ISO_DIR))
	$(MAKE) -C scripts qemu


clean:
	@echo "Cleaning build and bin directories..."
	@rm -rf $(OUT_DIR)

include scripts/Makefile