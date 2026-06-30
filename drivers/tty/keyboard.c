/**
 * @file keyboard.c
 * @brief TTY keyboard handler — keymap, modifier tracking, input core handler
 */
#include <stdbool.h>
#include <stddef.h>
#include <hubble/ctype.h>
#include <hubble/input.h>
#include <hubble/module.h>
#include <smp/scheduler.h>
#include <smp/waitqueue.h>
#include <lib/misc.k.h>
#include <drivers/tty/tty.h>
#include <drivers/tty/keyboard.h>
#include <drivers/tty/keymap.h>
#include "keymap.h"
#include "keyboard.h"

/* ── Keymap table ───────────────────────────────────────── */

const keymap_entry_t keymap[] = {
    {.id = {KEY_A, false}, 'a', 'A'},
    {.id = {KEY_B, false}, 'b', 'B'},
    {.id = {KEY_C, false}, 'c', 'C'},
    {.id = {KEY_D, false}, 'd', 'D'},
    {.id = {KEY_E, false}, 'e', 'E'},
    {.id = {KEY_F, false}, 'f', 'F'},
    {.id = {KEY_G, false}, 'g', 'G'},
    {.id = {KEY_H, false}, 'h', 'H'},
    {.id = {KEY_I, false}, 'i', 'I'},
    {.id = {KEY_J, false}, 'j', 'J'},
    {.id = {KEY_K, false}, 'k', 'K'},
    {.id = {KEY_L, false}, 'l', 'L'},
    {.id = {KEY_M, false}, 'm', 'M'},
    {.id = {KEY_N, false}, 'n', 'N'},
    {.id = {KEY_O, false}, 'o', 'O'},
    {.id = {KEY_P, false}, 'p', 'P'},
    {.id = {KEY_Q, false}, 'q', 'Q'},
    {.id = {KEY_R, false}, 'r', 'R'},
    {.id = {KEY_S, false}, 's', 'S'},
    {.id = {KEY_T, false}, 't', 'T'},
    {.id = {KEY_U, false}, 'u', 'U'},
    {.id = {KEY_V, false}, 'v', 'V'},
    {.id = {KEY_W, false}, 'w', 'W'},
    {.id = {KEY_X, false}, 'x', 'X'},
    {.id = {KEY_Y, false}, 'y', 'Y'},
    {.id = {KEY_Z, false}, 'z', 'Z'},

    {.id = {KEY_1, false}, '1', '!'},
    {.id = {KEY_2, false}, '2', '@'},
    {.id = {KEY_3, false}, '3', '#'},
    {.id = {KEY_4, false}, '4', '$'},
    {.id = {KEY_5, false}, '5', '%'},
    {.id = {KEY_6, false}, '6', '^'},
    {.id = {KEY_7, false}, '7', '&'},
    {.id = {KEY_8, false}, '8', '*'},
    {.id = {KEY_9, false}, '9', '('},
    {.id = {KEY_0, false}, '0', ')'},

    {.id = {KEY_SPACE, false}, ' ', ' '},
    {.id = {KEY_ENTER, false}, '\n', '\n'},
    {.id = {KEY_TAB, false}, '\t', '\t'},
    {.id = {KEY_ESC, false}, 27, 27},
    {.id = {KEY_BACKSPACE, false}, '\b', '\b'},

    {.id = {KEY_MINUS, false}, '-', '_'},
    {.id = {KEY_EQUAL, false}, '=', '+'},
    {.id = {KEY_LEFT_BRACKET, false}, '[', '{'},
    {.id = {KEY_RIGHT_BRACKET, false}, ']', '}'},
    {.id = {KEY_BACKSLASH, false}, '\\', '|'},
    {.id = {KEY_SEMICOLON, false}, ';', ':'},
    {.id = {KEY_APOSTROPHE, false}, '\'', '\"'},
    {.id = {KEY_COMMA, false}, ',', '<'},
    {.id = {KEY_PERIOD, false}, '.', '>'},
    {.id = {KEY_SLASH, false}, '/', '?'},
    {.id = {KEY_CAPS_LOCK, false}, 0, 0},
    {.id = {KEY_GRAVE, false}, '`', '~'},

    {.id = {KEY_LEFT_SHIFT, false}, 0, 0},
    {.id = {KEY_RIGHT_SHIFT, false}, 0, 0},
    {.id = {KEY_LEFT_CTRL, false}, 0, 0},
    {.id = {KEY_RIGHT_CTRL, true}, 0, 0},
    {.id = {KEY_LEFT_ALT, false}, 0, 0},
    {.id = {KEY_RIGHT_ALT, true}, 0, 0},
};

const size_t keymap_size = SIZEOF_ARRAY(keymap);

/* ── Keymap lookup ──────────────────────────────────────── */

char keymap_lookup_char(uint8_t scancode, bool extended, bool shift, bool caps)
{
	for (size_t i = 0; i < keymap_size; ++i)
		if (keymap[i].id.scancode == scancode && keymap[i].id.extended == extended)
		{
			char c = shift ? keymap[i].shifted : keymap[i].normal;

			if (isalpha(c) && caps)
				c = shift ? tolower(c) : toupper(c);

			return c;
		}
	return 0;
}

/* ── Modifier state tracking ────────────────────────────── */

typedef struct
{
	bool shift;
	bool ctrl;
	bool alt;
	bool caps_lock;
} kbd_state_t;

static kbd_state_t kbd_state = {0};

static void update_modifiers(uint16_t code, bool pressed)
{
	uint8_t sc = code & 0xFF;
	bool extended = code & 0x100;

	if (sc == KEY_LEFT_SHIFT || sc == KEY_RIGHT_SHIFT)
		kbd_state.shift = pressed;
	else if ((sc == KEY_LEFT_CTRL && !extended) ||
		 (sc == KEY_RIGHT_CTRL && extended))
		kbd_state.ctrl = pressed;
	else if ((sc == KEY_LEFT_ALT && !extended) ||
		 (sc == KEY_RIGHT_ALT && extended))
		kbd_state.alt = pressed;
	else if (sc == KEY_CAPS_LOCK && pressed)
		kbd_state.caps_lock = !kbd_state.caps_lock;
}

/* ── Key event handler ──────────────────────────────────── */

static void tty_handle_key(uint16_t code, bool pressed)
{
	uint8_t sc = code & 0xFF;
	bool extended = code & 0x100;

	if (kbd_state.ctrl && !extended)
	{
		if (!pressed)
			return;
		switch (sc)
		{
		case KEY_C:
			tty_input_char(tty_current, 0x03);
			return;
		case KEY_D:
			tty_input_char(tty_current, 0x04);
			return;
		case KEY_L:
			tty_input_char(tty_current, '\f');
			return;
		}
	}

	if (!pressed)
		return;

	if (extended)
	{
		switch (sc)
		{
		case KEY_UP:
			tty_write(tty_current, "\x1b[A", 3);
			return;
		case KEY_DOWN:
			tty_write(tty_current, "\x1b[B", 3);
			return;
		case KEY_RIGHT:
			tty_write(tty_current, "\x1b[C", 3);
			return;
		case KEY_LEFT:
			tty_write(tty_current, "\x1b[D", 3);
			return;
		case KEY_HOME:
			tty_write(tty_current, "\x1b[H", 3);
			return;
		case KEY_END:
			tty_write(tty_current, "\x1b[F", 3);
			return;
		case KEY_DELETE:
			tty_write(tty_current, "\x1b[3~", 4);
			return;
		case KEY_INSERT:
			tty_write(tty_current, "\x1b[2~", 4);
			return;
		case KEY_PAGEUP:
			tty_write(tty_current, "\x1b[5~", 4);
			return;
		case KEY_PAGEDOWN:
			tty_write(tty_current, "\x1b[6~", 4);
			return;
		default:
			return;
		}
	}

	if (sc >= KEY_F1 && sc <= KEY_F10)
		return;

	char c = keymap_lookup_char(sc, extended, kbd_state.shift, kbd_state.caps_lock);
	if (c != 0)
		tty_input_char(tty_current, c);
}

/* ── Input handler ──────────────────────────────────────── */

static bool tty_kbd_match(input_handler_t *handler, input_dev_t *dev)
{
	(void)handler;
	return input_test_bit(EV_KEY, dev->evbit);
}

static input_handle_t tty_kbd_handle;

static int tty_kbd_connect(input_handler_t *handler, input_dev_t *dev)
{
	tty_kbd_handle.dev = dev;
	tty_kbd_handle.handler = handler;
	tty_kbd_handle.private = NULL;

	input_link_handle(&tty_kbd_handle);
	return 0;
}

static void tty_kbd_disconnect(input_handle_t *handle)
{
	input_unlink_handle(handle);
}

static void tty_kbd_event(input_handle_t *handle, input_raw_event_t *ev)
{
	(void)handle;

	if (ev->type != EV_KEY)
		return;
	if (!tty_current)
		return;

	bool pressed = ev->value == 1;
	bool repeat = ev->value == 2;

	update_modifiers(ev->code, pressed);

	if (pressed || repeat)
		tty_handle_key(ev->code, true);
	else
		tty_handle_key(ev->code, false);
}

static input_handler_t tty_kbd_handler = {
    .name = "tty-keyboard",
    .match = tty_kbd_match,
    .connect = tty_kbd_connect,
    .disconnect = tty_kbd_disconnect,
    .event = tty_kbd_event,
};

/* ── Initcall ───────────────────────────────────────────── */

static int tty_keyboard_initcall(void)
{
	input_register_handler(&tty_kbd_handler);
	return 0;
}

module_init(tty_keyboard_initcall);
MODULE_NAME("tty_keyboard");
