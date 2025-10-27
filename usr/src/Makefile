TCC = ../bin/tcc
OUT = ../bin
SRC = start.c hello.c
CFLAGS = -ffreestanding -nostdlib -m64 -I../include
LDFLAGS = ../lib/libc.a

all: $(OUT)/hello

$(OUT)/hello: $(SRC)
	@mkdir -p $(OUT)
	$(TCC) $(CFLAGS) $^ $(LDFLAGS) -o $@
	@echo "Built $@"

clean:
	rm -f $(OUT)/hello
