# usr/Makefile
Q = @
export Q

OUT_DIR ?= $(abspath ../out)
BUILD_DIR := $(OUT_DIR)/build/usr
export OUT_DIR BUILD_DIR

USR_DIR := $(abspath .)
export USR_DIR

# CFLAGS для userland
CFLAGS := -ffreestanding -O2 -Wall -Wextra -m64 -mcmodel=large -g
export CFLAGS

# BUILD_TOOL
BUILD_TOOL := $(OUT_DIR)/tools/dev/build/build_main
BUILD_TOOL_FLAGS := --log-file $(OUT_DIR)/logs/usr_build.log -v
export BUILD_TOOL BUILD_TOOL_FLAGS

# Подкаталоги для сборки: сначала lib, потом apps
subdirs := lib apps

.PHONY: all build clean rebuild

all: build

build: build-tool
	@set -e; \
	for dir in $(subdirs); do \
		$(MAKE) -C $$dir BUILD_TOOL_FLAGS="$(BUILD_TOOL_FLAGS)"; \
	done
	@echo "Userland build complete"

build-tool:
	@$(MAKE) -C ../tools/dev/build build

clean:
	@rm -rf $(BUILD_DIR)
	@echo "Userland clean complete"

rebuild: clean build
