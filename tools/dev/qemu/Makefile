SRC_DIR := src

CC := gcc
CCFLAGS := 
INCLUDES := -Iinclude -I$(SRC_DIR)

LDFLAGS := -lncurses

override BUILD_DIR := $(if $(OUT_DIR),$(OUT_DIR)/tools/dev/qemu,build)

TARGET := $(BUILD_DIR)/qemu

SRCS := $(shell find $(SRC_DIR) -name "*.c")
OBJS := $(SRCS:%.c=$(BUILD_DIR)/%.o)

OVMF_SRC := ovmf
OVMF_DST := $(BUILD_DIR)/ovmf

.PHONY: all build run clean

all: build run

build: $(TARGET)
	@mkdir -p $(OVMF_DST)
	@cp -r $(OVMF_SRC)/* $(OVMF_DST)/

$(TARGET): $(OBJS)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CCFLAGS) $(INCLUDES) $(OBJS) $(LDFLAGS) -o $(TARGET)

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CCFLAGS) $(INCLUDES) -c $< -o $@

run: build
	@$(TARGET)

clean:
	@rm -rf $(BUILD_DIR)


# CC         := gcc
# CCFLAGS    :=
# INCLUDES   := -Iinclude -I$(SRC_DIR)
# LDFLAGS    := -lncurses

# override BUILD_DIR := $(if $(OUT_DIR),$(OUT_DIR)/tools/qemu,build)
# SRC_DIR    := src
# TARGET     := $(BUILD_DIR)/qemu

# OVMF_SRC := ovmf
# OVMF_DST := $(BUILD_DIR)/ovmf

# .PHONY: all build run

# all: build run

# build: $(BUILD_TOOL)
# 	@echo "🔨 Building menu with BUILD_TOOL..."
# 	$(BUILD_TOOL) $(BUILD_TOOL_FLAGS) \
# 		--src-dir $(SRC_DIR) \
# 		--build-dir $(BUILD_DIR) \
# 		--cc $(CC) \
# 		--cflags "$(CCFLAGS)" \
# 		--includes "$(INCLUDES)" \
# 		--output $(TARGET) \
# 		--type exe \
# 		--ldflags "$(LDFLAGS)" \
# 		--log-file $(LOG_FILE) \
# 		-v
# 	@mkdir -p $(OVMF_DST)
# 	@cp -r $(OVMF_SRC)/* $(OVMF_DST)/

# run: build
# 	@$(TARGET)
