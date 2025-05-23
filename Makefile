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

clean:
	@echo "Cleaning build and bin directories..."
	@rm -rf $(OUT_DIR)

img:
	@echo "Creating ISO..."
	@sh ./scripts/make-image.sh

flash:
	@echo "Flashing ISO..."
	@sh ./scripts/make-flash.sh

help:
	@echo "Usage:"
	@sh ./scripts/make-help.sh
