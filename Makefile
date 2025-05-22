# Root Makefile

# Allow override from command line
ARCH ?= x86
BUILD_DIR := build
ARCH_DIR := arch/$(ARCH)

ISO_DIR := iso

.PHONY: all clean run build help qemu

all: build

build:
	@echo "Building kernel..."
	$(MAKE) -C $(ARCH_DIR) BUILD_DIR=$(abspath $(BUILD_DIR)/$(ARCH)) ISO_DIR=$(abspath $(ISO_DIR)/$(ARCH))

run:
	@echo "Running kernel..."
	$(MAKE) -C $(ARCH_DIR) run BUILD_DIR=$(abspath $(BUILD_DIR)/$(ARCH)) ISO_DIR=$(abspath $(ISO_DIR)/$(ARCH))

clean:
	@echo "Cleaning build and bin directories..."
	@rm -rf $(BUILD_DIR) $(ISO_DIR)

help:
	@echo "Usage:"
	@echo "  make all - to build and run"
	@echo "  make run - to run the kernel"
	@echo "  make clean - to clean the build directory"
	@echo "  make help - to print this help message"

QEMU = qemu-system-x86_64
QEMU_FLAGs = -M pc \
  -cpu Haswell \
  -smp 2 \
  -m 1024 \
  -drive if=pflash,format=raw,readonly=on,file=ovmf/OVMF_CODE.fd \
  -drive if=pflash,format=raw,file=ovmf/OVMF_VARS.fd \
  -hda fat:rw:$(ISO_DIR) \
  -serial stdio

qemu:
	$(QEMU) $(QEMU_FLAGS)
