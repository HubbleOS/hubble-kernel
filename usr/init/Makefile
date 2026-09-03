# usr/init/Makefile
# Build /init as a minimal static ELF binary.
# Does NOT depend on the build tool - single file, direct compile.

CC      ?= x86_64-elf-gcc
CFLAGS  ?= -ffreestanding -O2 -Wall -Wextra -m64 -g -fno-stack-protector
OUT_DIR ?= ../../out

SRC      := init.c
LDSCRIPT := link.ld
OUTPUT   := $(OUT_DIR)/init/init.elf

INCLUDES := -I$(OUT_DIR)/../usr/include

.PHONY: all clean

all: $(OUTPUT)

$(OUTPUT): $(SRC) $(LDSCRIPT)
	@mkdir -p $(dir $@)
	$(CC) -ffreestanding -O2 -nostdlib -static -nostartfiles \
		-T $(LDSCRIPT) $(CFLAGS) $(INCLUDES) \
		-o $@ $(SRC)
	@echo "Built /init -> $@"

clean:
	@rm -f $(OUTPUT)
