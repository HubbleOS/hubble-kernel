SRC_DIR := src

CC := gcc
CCFLAGS := -MMD -MP
INCLUDES := -Iinclude -I$(SRC_DIR)

LDFLAGS := 

override BUILD_DIR := $(if $(OUT_DIR),$(OUT_DIR)/tools/dev/build,build)

TARGET := $(BUILD_DIR)/build_main

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

