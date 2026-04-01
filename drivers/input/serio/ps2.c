#include <stdbool.h>
#include "io.h"
#include "ps2.h"

#define PS2_DATA 0x60
#define PS2_STATUS 0x64
#define PS2_COMMAND 0x64

void ps2_wait_input(void) // wait until we can write
{
	while (inb(PS2_STATUS) & 0x02)
		;
}

void ps2_wait_output(void) // wait until we can read
{
	while (!(inb(PS2_STATUS) & 0x01))
		;
}

static bool init_done = false;

void ps2_init()
{
	if (init_done)
		return;

	__asm__ volatile("cli");

	// 1. Disable devices
	ps2_wait_input();
	outb(PS2_COMMAND, 0xAD);
	ps2_wait_input();
	outb(PS2_COMMAND, 0xA7);

	// 2. Flush output buffer
	while (inb(PS2_STATUS) & 1)
		inb(PS2_DATA);

	// 3. Read config byte
	ps2_wait_input();
	outb(PS2_COMMAND, 0x20);
	ps2_wait_output();
	uint8_t config = inb(PS2_DATA);

	// 4. Disable IRQs + translation
	config &= ~0x03;
	config &= ~0x40;

	ps2_wait_input();
	outb(PS2_COMMAND, 0x60);
	ps2_wait_input();
	outb(PS2_DATA, config);

	// 5. Enable first port
	ps2_wait_input();
	outb(PS2_COMMAND, 0xAE);

	// 6. Enable keyboard scanning
	ps2_wait_input();
	outb(PS2_DATA, 0xF4);
	ps2_wait_output();
	uint8_t ack = inb(PS2_DATA);

	// 7. Re-enable IRQ1 in config  ← THIS WAS MISSING
	ps2_wait_input();
	outb(PS2_COMMAND, 0x20);
	ps2_wait_output();
	config = inb(PS2_DATA);

	config |= 0x01;	 // enable IRQ1
	config |= 0x02;	 // enable IRQ12 (mouse)
	config &= ~0x40; // keep translation disabled

	ps2_wait_input();
	outb(PS2_COMMAND, 0x60);
	ps2_wait_input();
	outb(PS2_DATA, config);

	init_done = true;

	__asm__ volatile("sti");
}
