CC         := gcc
CCFLAGS    :=
INCLUDES   := -Iinclude -Imenu
LDFLAGS    := -lncurses

override BUILD_DIR := $(if $(OUT_DIR),$(OUT_DIR)/tools/menu,build)
SRC_DIR    := menu
TARGET     := $(BUILD_DIR)/main

.PHONY: all build run

all: build-tools build

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

subdirs += debug
subdirs += stats
subdirs += img
subdirs += qemu

.PHONY: build-tools
build-tools:
	$(Q)set -e; \
	for dir in $(subdirs); do \
		$(MAKE) -C $$dir; \
	done

.PHONY: run
run:
	@$(TARGET)
