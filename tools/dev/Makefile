CC         := gcc
CCFLAGS    :=
INCLUDES   := -Iinclude -Imenu
LDFLAGS    := -lncurses

override BUILD_DIR := $(if $(OUT_DIR),$(OUT_DIR)/tools/menu,build)
SRC_DIR    := menu
TARGET     := $(BUILD_DIR)/main

.PHONY: all build run clean

all: build run

build: $(BUILD_TOOL)
	@echo "🔨 Building menu with BUILD_TOOL..."
	$(BUILD_TOOL) $(BUILD_TOOL_FLAGS) \
		--src-dir $(SRC_DIR) \
		--build-dir $(BUILD_DIR) \
		--cc $(CC) \
		--cflags "$(CCFLAGS)" \
		--includes "$(INCLUDES)" \
		--output $(TARGET) \
		--type exe \
		--ldflags "$(LDFLAGS)" \
		--log-file $(LOG_FILE) \
		-v

run: build
	@$(TARGET)
