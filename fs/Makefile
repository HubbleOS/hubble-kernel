FS_DIR := $(abspath $(ROOT_DIR)/fs)
FS_BUILD_DIR := $(BUILD_DIR)/fs
FS_LIB := $(FS_BUILD_DIR)/libfs.a
export FS_DIR FS_BUILD_DIR FS_LIB

.PHONY: build

build: $(BUILD_TOOL)
	$(Q)$(BUILD_TOOL) $(BUILD_TOOL_FLAGS) \
		--src-dir $(FS_DIR) \
		--build-dir $(FS_BUILD_DIR) \
		--cc $(CC) \
		--cflags "$(CFLAGS)" \
		--includes "$(INCLUDES)" \
		--ar $(AR) \
		--output $(FS_LIB) \
		--type archive
