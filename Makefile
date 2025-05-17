# Root Makefile

# Allow override from command line
ARCH ?= x86
BUILD_DIR := build/$(ARCH)
ARCH_DIR := arch/$(ARCH)

.PHONY: all clean run build help

all: build

build:
	$(MAKE) -C $(ARCH_DIR) BUILD_DIR=$(abspath $(BUILD_DIR))

run:
	$(MAKE) -C $(ARCH_DIR) run BUILD_DIR=$(abspath $(BUILD_DIR))

clean:
	$(MAKE) -C $(ARCH_DIR) clean BUILD_DIR=$(abspath $(BUILD_DIR))

help:
	@echo "Usage:"
	@echo "  make all - to build and run"
	@echo "  make run - to run the kernel"
	@echo "  make clean - to clean the build directory"
	@echo "  make help - to print this help message"
