CC := gcc
CCFLAGS := 
INCLUDES := -Iinclude -Isrc

LDFLAGS := -lncurses

# BUILD_DIR ?= build
# BUILD_DIR := $(if $(OUT_DIR),$(OUT_DIR)/qemu,build)
override BUILD_DIR := $(if $(OUT_DIR),$(OUT_DIR)/tools/qemu,build)

# SRC_DIR := src
SRC_DIR := .
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
	@$(TARGET) $(ISO_DIR)

clean:
	@rm -rf $(BUILD_DIR)
