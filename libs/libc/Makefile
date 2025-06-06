LIBC_DIR := src
LIBC_SRC := $(shell find $(LIBC_DIR) -type f -name "*.c")
LIBC_OBJ := $(patsubst %.c,$(BUILD_DIR)/%.o,$(LIBC_SRC))

INCLUDES += -Iinclude

LIBC_OUT := $(BUILD_DIR)/libc.a

.PHONY: all

all: $(LIBC_OUT)
	@echo "✅ Dummy libc build complete"

$(BUILD_DIR)/%.o: %.c
	@echo "Compiling: $< -> $@"
	@mkdir -p $(dir $@)
	$(CC) -c $(CFLAGS) $(INCLUDES) $< -o $@

$(LIBC_OUT): $(LIBC_OBJ)
	@echo "Archiving libc.a..."
	@mkdir -p $(dir $@)
	$(AR) rcs $@ $^
