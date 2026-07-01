/**
 * @file mouse.c
 * @brief PS/2 mouse driver — IRQ handler, initialisation, VFS callbacks
 */
#include <stdint.h>
#include <stddef.h>
#include <io.h>
#include <higher_half.h>
#include <hubble/string.h>
#include <hubble/printk.h>
#include <hubble/module.h>
#include <mm/vmm.h>
#include <mm/pmm.h>
#include <mm/kmalloc.h>
#include <interrupt/interrupt.h>
#include "ps2.h"
#include "mouse.h"

/* ── Global state ───────────────────────────────────────── */

static mouse_t *mouse_g = NULL;
static uint8_t mouse_packet[3];
static uint8_t mouse_cycle = 0;

/* ── VFS callbacks ──────────────────────────────────────── */

mouse_t *get_mouse_info(void)
{
	return mouse_g;
}

uint64_t mouse_mmap(uint64_t offset, size_t size)
{
	(void)offset;
	(void)size;
	return (uint64_t)virt_to_phys((uint64_t)mouse_g);
}

uint64_t mouse_read_file(uint64_t offset, size_t size, void *buf)
{
	(void)offset;
	memcpy(buf, mouse_g, size);
	return size;
}

/* ── IRQ handler ────────────────────────────────────────── */

void mouse_handler(registers_t *regs)
{
	(void)regs;
	if (!(inb(PS2_COMMAND) & 0x20))
		return;

	mouse_packet[mouse_cycle++] = inb(PS2_DATA);

	if (mouse_cycle < 3)
		return;

	mouse_cycle = 0;

	uint8_t flags = mouse_packet[0];

	if (!(flags & 0x08))
		return;

	if ((flags & 0x40) || (flags & 0x80))
		return;

	int32_t x = mouse_packet[1] - ((flags & 0x10) ? 256 : 0);
	int32_t y = mouse_packet[2] - ((flags & 0x20) ? 256 : 0);

	mouse_g->x += x;
	mouse_g->y -= y;

	mouse_g->left = (flags & 0x01) != 0;
	mouse_g->right = (flags & 0x02) != 0;
	mouse_g->middle = (flags & 0x04) != 0;

	static uint8_t prev_buttons = 0;
	uint8_t curr_buttons = flags & 0x07;

	if ((curr_buttons & 0x01) && !(prev_buttons & 0x01))
		mouse_g->left_clicked = true;
	if ((curr_buttons & 0x02) && !(prev_buttons & 0x02))
		mouse_g->right_clicked = true;

	if (!(curr_buttons & 0x01))
		mouse_g->left_clicked = false;
	if (!(curr_buttons & 0x02))
		mouse_g->right_clicked = false;

	prev_buttons = curr_buttons;
}

/* ── Hardware helpers ───────────────────────────────────── */

static void mouse_write(uint8_t cmd)
{
	ps2_wait_input();
	outb(PS2_COMMAND, 0xD4);
	ps2_wait_input();
	outb(PS2_DATA, cmd);
}

static uint8_t mouse_read(void)
{
	ps2_wait_output();
	return inb(PS2_DATA);
}

void mouse_init(void)
{
	mouse_g = kmalloc(sizeof(mouse_t), GFP_KERNEL);
	if (!mouse_g)
	{
		printk(KERN_ERR "[MOUSE] Failed to allocate mouse state\n");
		return;
	}
	memset(mouse_g, 0, sizeof(mouse_t));

	__asm__ volatile("cli");

	ps2_wait_input();
	outb(PS2_COMMAND, 0xA8);

	ps2_wait_input();
	outb(PS2_COMMAND, 0x20);
	ps2_wait_output();
	uint8_t config = inb(PS2_DATA);

	config |= 0x01;
	config |= 0x02;
	config &= ~0x20;
	config &= ~0x40;

	ps2_wait_input();
	outb(PS2_COMMAND, PS2_DATA);
	ps2_wait_input();
	outb(PS2_DATA, config);

	mouse_write(0xFF);
	uint8_t ack = mouse_read();
	printk(KERN_INFO "Mouse reset ack: 0x%x (expect 0xFA)\n", ack);
	uint8_t bat = mouse_read();
	uint8_t id = mouse_read();
	printk(KERN_INFO "Mouse BAT: 0x%x, ID: 0x%x\n", bat, id);

	mouse_write(0xF6);
	mouse_read();

	mouse_write(0xF4);
	mouse_read();

	__asm__ volatile("sti");
	printk(KERN_OK "Mouse initialized\n");
}


