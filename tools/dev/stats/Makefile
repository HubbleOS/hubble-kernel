SRC_DIR    := src

CC         := gcc
CCFLAGS    :=
INCLUDES   := -Iinclude -I$(SRC_DIR)
LDFLAGS    := -lncurses

override BUILD_DIR := $(if $(OUT_DIR),$(OUT_DIR)/tools/dev/stats,build)
TARGET     := $(BUILD_DIR)/stats

.PHONY: all build run

all: build

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
	@$(TARGET)
