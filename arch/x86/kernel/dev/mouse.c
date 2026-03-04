
#include "printk.h"
#include <interrupt/interrupt.h>
#include <stdint.h>
#include <io.h>
#include "ps2.h"
#include <stddef.h>
#include <mm/kmalloc.h>
#include "mouse.h"

static mouse_t *mouse_g = NULL;
static uint8_t mouse_packet[2];
static uint8_t mouse_cycle = 0;
mouse_t *get_mouse_info(void)
{

	return mouse_g;
}

void mouse_handler(registers_t *regs)
{
	if (!(inb(PS2_COMMAND) & 0x20))
		return;
	mouse_packet[mouse_cycle++] = inb(PS2_DATA);

	if (mouse_cycle < 3)
		return;

	mouse_cycle = 0;

	uint8_t flags = mouse_packet[0];

	// Sync check
	if (!(flags & 0x08))
		return;

	// Discard overflow packets
	if ((flags & 0x40) || (flags & 0x80))
		return;

	// Correct sign extension
	int32_t x = mouse_packet[1] - ((flags & 0x10) ? 256 : 0);
	int32_t y = mouse_packet[2] - ((flags & 0x20) ? 256 : 0);

	mouse_g->x += x;
	mouse_g->y -= y; // invert Y

	mouse_g->left = (flags & 0x01) != 0;
	mouse_g->right = (flags & 0x02) != 0;
	mouse_g->middle = (flags & 0x04) != 0;

	// Detect clicks (button just pressed this packet)
	static uint8_t prev_buttons = 0;
	uint8_t curr_buttons = flags & 0x07;

	if ((curr_buttons & 0x01) && !(prev_buttons & 0x01))
		mouse_g->left_clicked = true; // left just pressed
	if ((curr_buttons & 0x02) && !(prev_buttons & 0x02))
		mouse_g->right_clicked = true; // right just pressed

	// Clear clicks (button just released this packet)
	if (!(curr_buttons & 0x01))
		mouse_g->left_clicked = false; // left just released
	if (!(curr_buttons & 0x02))
		mouse_g->right_clicked = false; // right just released

	prev_buttons = curr_buttons;

	if (mouse_g->x < 0)
		mouse_g->x = 0;
	if (mouse_g->y < 0)
		mouse_g->y = 0;
	if (mouse_g->x > 1920)
		mouse_g->x = 1920;
	if (mouse_g->y > 1080)
		mouse_g->y = 1080;
	outb(0x3f8, 'M');
}

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

void mouse_init()
{
	__asm__ volatile("cli");

	// 1. Enable second PS/2 port
	ps2_wait_input();
	outb(PS2_COMMAND, 0xA8);

	// 3. Read config, enable IRQ12 and mouse clock
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

	// 4. Reset mouse
	mouse_write(0xFF);
	uint8_t ack = mouse_read();
	printk("Mouse reset ack: 0x%x (expect 0xFA)\n", ack);
	uint8_t bat = mouse_read();
	uint8_t id = mouse_read();
	printk("Mouse BAT: 0x%x, ID: 0x%x\n", bat, id);

	// 5. Set defaults
	mouse_write(0xF6);
	mouse_read();

	// 6. Enable data reporting
	mouse_write(0xF4);
	mouse_read();

	irq_install_handler(12, mouse_handler);

	mouse_g = (mouse_t *)kmalloc(sizeof(mouse_t), GFP_KERNEL);
	mouse_g->x = 1920 / 2;
	mouse_g->y = 1080 / 2;

	__asm__ volatile("sti");
	printk("Mouse initialized\n");
}
