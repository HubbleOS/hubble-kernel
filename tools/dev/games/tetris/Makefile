SRC_DIR    := src

CC         := gcc
CCFLAGS    :=
INCLUDES   := -Iinclude -I$(SRC_DIR)
LDFLAGS    := -lncurses

override BUILD_DIR := $(if $(OUT_DIR),$(OUT_DIR)/tools/menu,build)
TARGET     := $(BUILD_DIR)/main

.PHONY: all build run

all: build

BUILD_TOOL := ../../../../out/tools/dev/build/build_main

build: $(BUILD_TOOL)
	$(Q)$(BUILD_TOOL) $(BUILD_TOOL_FLAGS) \
		--src-dir $(SRC_DIR) \
		--build-dir $(BUILD_DIR) \
		--cc $(CC) \
		--cflags "$(CCFLAGS)" \
		--includes "$(INCLUDES)" \
		--output $(TARGET) \
		--type exe \
		--ldflags "$(LDFLAGS)"

run: build
	$(Q)$(TARGET)
