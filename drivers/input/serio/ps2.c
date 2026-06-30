/**
 * @file ps2.c
 * @brief PS/2 controller initialisation and port I/O helpers
 */
#include <stdbool.h>
#include "io.h"
#include "ps2.h"

/* ── Port I/O (provided by arch-level io.h) ─────────────── */

void ps2_wait_input(void)
{
	while (inb(PS2_STATUS) & 0x02)
		;
}

void ps2_wait_output(void)
{
	while (!(inb(PS2_STATUS) & 0x01))
		;
}

/* ── Controller initialisation ──────────────────────────── */

static bool init_done = false;

void ps2_init(void)
{
	if (init_done)
		return;

	__asm__ volatile("cli");

	/* 1. Disable devices */
	ps2_wait_input();
	outb(PS2_COMMAND, 0xAD);
	ps2_wait_input();
	outb(PS2_COMMAND, 0xA7);

	/* 2. Flush output buffer */
	while (inb(PS2_STATUS) & 1)
		inb(PS2_DATA);

	/* 3. Read config byte */
	ps2_wait_input();
	outb(PS2_COMMAND, 0x20);
	ps2_wait_output();
	uint8_t config = inb(PS2_DATA);

	/* 4. Disable IRQs + translation */
	config &= ~0x03;
	config &= ~0x40;

	ps2_wait_input();
	outb(PS2_COMMAND, 0x60);
	ps2_wait_input();
	outb(PS2_DATA, config);

	/* 5. Enable first port */
	ps2_wait_input();
	outb(PS2_COMMAND, 0xAE);

	/* 6. Enable keyboard scanning */
	ps2_wait_input();
	outb(PS2_DATA, 0xF4);
	ps2_wait_output();
	uint8_t ack = inb(PS2_DATA);
	(void)ack;

	/* 7. Re-enable IRQ1 in config */
	ps2_wait_input();
	outb(PS2_COMMAND, 0x20);
	ps2_wait_output();
	config = inb(PS2_DATA);

	config |= 0x01;  /* enable IRQ1 */
	config |= 0x02;  /* enable IRQ12 (mouse) */
	config &= ~0x40; /* keep translation disabled */

	ps2_wait_input();
	outb(PS2_COMMAND, 0x60);
	ps2_wait_input();
	outb(PS2_DATA, config);

	init_done = true;

	__asm__ volatile("sti");
}
