ARCH     ?= x86_64
CROSS    ?= x86_64-elf-
CC       := $(CROSS)gcc
LD       := $(CROSS)ld
OBJCOPY  := $(CROSS)objcopy

BUILD_DIR := ../../build/usr
OUT_DIR   := ../bin

SRC := user_main.c
OUT := $(OUT_DIR)/user.elf

# include и libc
CFLAGS   := -ffreestanding -fno-pic -m64 -O2 -nostartfiles -static -I../include
LDFLAGS  := -Tuser.ld
LIBS     := -L../lib -lc

all: $(OUT)

$(OUT): $(SRC) user.ld
	@mkdir -p $(OUT_DIR)
	@echo "Compiling user ELF: $@"
	$(CC) $(CFLAGS) -o $(OUT) $(SRC) $(LIBS) $(LDFLAGS)

clean:
	rm -rf $(OUT_DIR) $(BUILD_DIR)
