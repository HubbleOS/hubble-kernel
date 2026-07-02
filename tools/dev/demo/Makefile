# tools/dev/demo/Makefile

SRC_DIR    := src
CC         := gcc
CCFLAGS    :=
INCLUDES   := -Iinclude -I$(SRC_DIR)
LDFLAGS    := -lncurses

override BUILD_DIR := $(if $(OUT_DIR),$(OUT_DIR)/tools/dev/demo,build)
TARGET     := demo
BIN        := $(BUILD_DIR)/$(TARGET)

.PHONY: all build run clean

all: build

build:
	@mkdir -p $(BUILD_DIR)
	@cmake -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=Debug \
		-DCMAKE_C_FLAGS="" -DCMAKE_CXX_FLAGS="" -DARCH_DIR=$(ARCH_DIR)
	@cmake --build $(BUILD_DIR) --target $(TARGET)

run: build
	@$(MAKE) -C $(BUILD_DIR)
	@$(BIN)

clean:
	@rm -rf $(BUILD_DIR)
