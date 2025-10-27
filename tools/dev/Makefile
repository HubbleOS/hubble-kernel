CC := gcc
CCFLAGS := 
INCLUDES := -Iinclude -Imenu

LDFLAGS := -lncurses

override BUILD_DIR := $(if $(OUT_DIR),$(OUT_DIR)/tools/menu,build)

SRC_DIR := menu
TARGET := $(BUILD_DIR)/main

SRCS := $(shell find $(SRC_DIR) -name "*.c")
OBJS := $(SRCS:%.c=$(BUILD_DIR)/%.o)

.PHONY: all build run clean

all: build run

build: $(TARGET)

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
