// #include "ps2.h"
// #include "keyboard.h"

// #include <stddef.h>
// #include <hubble/string.h>
// #include <hubble/init.h>
// #include <hubble/printk.h>

// #include <io.h>

// #include <drivers/tty/keyboard.h>
// #include <drivers/tty/keymap.h>

// #include <asm.h>
// #include <smp/waitqueue.h>

// static bool process_scancode_once(uint8_t raw, key_event_t *out_evt)
// {
// 	static bool extended = false;
// 	static bool shift_pressed = false;
// 	static bool ctrl_pressed = false;
// 	static bool alt_pressed = false;
// 	static bool caps_lock_active = false;

// 	uint8_t scancode = GET_SCANCODE(raw);

// 	bool released = IS_RELEASED(raw);

// 	if (IS_EXTENDED(raw))
// 	{
// 		// mark that next scancode is extended; don't emit event yet
// 		extended = true;
// 		return false;
// 	}

// 	// handle modifiers
// 	if (scancode == KEY_LEFT_SHIFT || scancode == KEY_RIGHT_SHIFT)
// 	{
// 		shift_pressed = !released;
// 	}
// 	else if ((scancode == KEY_LEFT_CTRL && !extended) || (scancode == KEY_RIGHT_CTRL && extended))
// 	{
// 		ctrl_pressed = !released;
// 	}
// 	else if ((scancode == KEY_LEFT_ALT && !extended) || (scancode == KEY_RIGHT_ALT && extended))
// 	{
// 		alt_pressed = !released;
// 	}
// 	else if (scancode == KEY_CAPS_LOCK && !released)
// 	{
// 		caps_lock_active = !caps_lock_active;
// 	}

// 	// prepare event
// 	out_evt->id.scancode = scancode;
// 	out_evt->id.extended = extended;
// 	out_evt->released = released;
// 	out_evt->is_shift = shift_pressed;
// 	out_evt->is_ctrl = ctrl_pressed;
// 	out_evt->is_alt = alt_pressed;
// 	out_evt->is_caps_lock = caps_lock_active;

// 	// reset extended flag after consuming
// 	extended = false;
// 	return true;
// }

// #define KBD_BUFFER_SIZE 128

// static key_event_t kbd_buffer[KBD_BUFFER_SIZE];
// static volatile size_t kbd_head = 0;
// static volatile size_t kbd_tail = 0;

// static void kbd_push(key_event_t e)
// {
// 	size_t next = (kbd_head + 1) % KBD_BUFFER_SIZE;
// 	if (next != kbd_tail)
// 	{
// 		kbd_buffer[kbd_head] = e;
// 		kbd_head = next;
// 	}
// }

// static bool kbd_pop(key_event_t *out)
// {
// 	if (kbd_tail == kbd_head)
// 		return false;
// 	*out = kbd_buffer[kbd_tail];
// 	kbd_tail = (kbd_tail + 1) % KBD_BUFFER_SIZE;
// 	return true;
// }

// typedef struct
// {
// 	task_t *waiting; // task blocked waiting for input
// } kbd_stream_t;

// static kbd_stream_t kbd_stream;

// bool keyboard_poll_event(key_event_t *ev)
// {
// 	cli();
// 	bool ok = kbd_pop(ev);
// 	sti();

// 	return ok;
// }

// key_event_t keyboard_get_event(void)
// {
// 	key_event_t ev;

// 	while (true)
// 	{
// 		cli();
// 		bool has_event = kbd_pop(&ev);
// 		sti();

// 		if (has_event && !ev.released)
// 			return ev;

// 		task_sleep();
// 	}
// }

// static wait_queue_t kbd_queue;

// char keyboard_get_char(void)
// {
// 	key_event_t ev;

// 	while (true)
// 	{
// 		bool has_event = kbd_pop(&ev);

// 		if (has_event)
// 		{
// 			if (ev.released)
// 				continue;

// 			return keymap_lookup_char(ev.id.scancode, ev.id.extended,
// 						  ev.is_shift, ev.is_caps_lock);
// 		}
// 		waitqueue_sleep(&kbd_queue);
// 	}
// }

// uint64_t kbd_read(uint64_t offset, size_t size, void *buf)
// {
// 	char c = keyboard_get_char();
// 	memcpy(buf, &c, size < 1 ? size : 1);
// 	return 1;
// }

// void keyboard_irq(registers_t *r)
// {
// 	uint8_t status = inb(0x64);

// 	if ((status & 0x20))
// 		return;
// 	if (!(status & 0x01))
// 		return;

// 	uint8_t raw = inb(0x60);

// 	key_event_t evt;
// 	if (process_scancode_once(raw, &evt))
// 	{
// 		kbd_push(evt);
// 		waitqueue_wake_all(&kbd_queue);
// 	}
// }

// static void keyboard_write(uint8_t cmd)
// {
// 	ps2_wait_input();
// 	outb(PS2_DATA, cmd);
// }

// static uint8_t keyboard_read(void)
// {
// 	ps2_wait_output();
// 	return inb(PS2_DATA);
// }

// void keyboard_init()
// {
// 	__asm__ volatile("cli");

// 	uint8_t act;
// 	keyboard_write(0xF0);
// 	act = keyboard_read();

// 	printk("Keyboard active: %02x\n", act);

// 	keyboard_write(0x01);
// 	act = keyboard_read();
// 	printk("Keyboard active: %02x\n", act);

// 	waitqueue_init(&kbd_queue);

// 	__asm__ volatile("sti");
// 	printk("Keyboard initialized\n");
// }

// static int keyboard_initcall(void)
// {
// 	ps2_init();
// 	keyboard_init();
// 	irq_install_handler(1, keyboard_irq);
// 	return 0;
// }

// device_initcall(keyboard_initcall);
#include "ps2.h"
#include "keyboard.h"

#include <stddef.h>
#include <hubble/string.h>
#include <hubble/module.h>
#include <hubble/printk.h>
#include <hubble/input.h>

#include <io.h>
#include <asm.h>
#include <smp/waitqueue.h>

#include <drivers/tty/keyboard.h>
#include <drivers/tty/keymap.h>

/* ── Scancode processing (без змін) ─────────────────────────────────────── */

static bool process_scancode_once(uint8_t raw, key_event_t *out_evt)
{
	static bool extended = false;
	static bool shift_pressed = false;
	static bool ctrl_pressed = false;
	static bool alt_pressed = false;
	static bool caps_lock_active = false;

	uint8_t scancode = GET_SCANCODE(raw);
	bool released = IS_RELEASED(raw);

	if (IS_EXTENDED(raw))
	{
		extended = true;
		return false;
	}

	if (scancode == KEY_LEFT_SHIFT || scancode == KEY_RIGHT_SHIFT)
		shift_pressed = !released;
	else if ((scancode == KEY_LEFT_CTRL && !extended) ||
		 (scancode == KEY_RIGHT_CTRL && extended))
		ctrl_pressed = !released;
	else if ((scancode == KEY_LEFT_ALT && !extended) ||
		 (scancode == KEY_RIGHT_ALT && extended))
		alt_pressed = !released;
	else if (scancode == KEY_CAPS_LOCK && !released)
		caps_lock_active = !caps_lock_active;

	out_evt->id.scancode = scancode;
	out_evt->id.extended = extended;
	out_evt->released = released;
	out_evt->is_shift = shift_pressed;
	out_evt->is_ctrl = ctrl_pressed;
	out_evt->is_alt = alt_pressed;
	out_evt->is_caps_lock = caps_lock_active;

	extended = false;
	return true;
}

/* ── input_dev ───────────────────────────────────────────────────────────── */

static input_dev_t kbd_input_dev = {
    .name = "ps2-keyboard",
};

/* ── IRQ ─────────────────────────────────────────────────────────────────── */

void keyboard_irq(registers_t *r)
{
	uint8_t status = inb(0x64);
	if ((status & 0x20))
		return;
	if (!(status & 0x01))
		return;

	uint8_t raw = inb(0x60);

	key_event_t evt;
	if (!process_scancode_once(raw, &evt))
		return;

	/*
	 * Пакуємо key_event_t в input_event_t і відправляємо в input core.
	 *
	 * type  = EV_KEY
	 * code  = scancode (потім можна замінити на KEY_* константи)
	 * value = 1 (press) / 0 (release)
	 *
	 * Модифікатори (shift/ctrl/alt/caps) передаємо окремими подіями
	 * — так само як Linux: кожна клавіша це окрема EV_KEY подія.
	 */
	input_raw_event_t ev = {
	    .type = EV_KEY,
	    .code = evt.id.scancode | (evt.id.extended ? 0x100 : 0),
	    .value = evt.released ? 0 : 1,
	};

	input_report(&kbd_input_dev, &ev);
}

/* ── Hardware init (без змін) ────────────────────────────────────────────── */

static void keyboard_write(uint8_t cmd)
{
	ps2_wait_input();
	outb(PS2_DATA, cmd);
}

static uint8_t keyboard_read(void)
{
	ps2_wait_output();
	return inb(PS2_DATA);
}

static void keyboard_init(void)
{
	__asm__ volatile("cli");

	uint8_t act;
	keyboard_write(0xF0);
	act = keyboard_read();
	printk(KERN_INFO "Keyboard active: %02x\n", act);

	keyboard_write(0x01);
	act = keyboard_read();
	printk(KERN_INFO "Keyboard active: %02x\n", act);

	__asm__ volatile("sti");
	printk(KERN_OK "Keyboard initialized\n");
}

/* ── initcall ────────────────────────────────────────────────────────────── */

static int keyboard_initcall(void)
{
	ps2_init();
	keyboard_init();

	/* виставляємо що вміє пристрій */
	input_set_bit(EV_KEY, kbd_input_dev.evbit);

	input_register_device(&kbd_input_dev); /* підключить handlers */
	irq_install_handler(1, keyboard_irq);

	return 0;
}

module_init(keyboard_initcall);
MODULE_NAME("serio_keyboard");
