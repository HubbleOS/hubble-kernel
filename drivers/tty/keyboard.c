#include "keymap.h"
#include "keyboard.h"

#include <stdbool.h>
#include <stddef.h>

#include <hubble/ctype.h>

#include <smp/scheduler.h>
#include <smp/waitqueue.h>

#include <lib/misc.k.h>

// Existing keymap (keep as is)
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

// Existing keymap_lookup_char (keep as is)
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

// input_event_t keyboard_get_input(void)
// {
// 	while (true)
// 	{
// 		key_event_t evt = keyboard_get_event();

// 		// input_event_t input = {0};

// 		input_event_t input = {
// 		    .type = KEY_TYPE_UNKNOWN,
// 		    .shift = evt.is_shift,
// 		    .ctrl = evt.is_ctrl,
// 		    .alt = evt.is_alt,
// 		    .character = 0,
// 		    .action = KEY_ACTION_NONE};

// 		// Skip modifier keys themselves
// 		if (evt.id.scancode == KEY_LEFT_SHIFT ||
// 		    evt.id.scancode == KEY_RIGHT_SHIFT ||
// 		    evt.id.scancode == KEY_LEFT_CTRL ||
// 		    (evt.id.scancode == KEY_RIGHT_CTRL && evt.id.extended) ||
// 		    evt.id.scancode == KEY_LEFT_ALT ||
// 		    (evt.id.scancode == KEY_RIGHT_ALT && evt.id.extended) ||
// 		    evt.id.scancode == KEY_CAPS_LOCK)
// 		{
// 			input.type = KEY_TYPE_MODIFIER;
// 			continue;
// 		}

// 		// Handle function keys
// 		if (!evt.id.extended && evt.id.scancode >= KEY_F1 && evt.id.scancode <= KEY_F12)
// 		{
// 			input.type = KEY_TYPE_FUNCTION;
// 			if (evt.id.scancode <= KEY_F10)
// 				input.function_key = evt.id.scancode - KEY_F1 + 1;
// 			else
// 				input.function_key = evt.id.scancode - KEY_F11 + 11;
// 			return input;
// 		}

// 		// Handle extended special keys (arrows, navigation)
// 		if (evt.id.extended)
// 		{
// 			input.type = KEY_TYPE_SPECIAL;

// 			switch (evt.id.scancode)
// 			{
// 			case KEY_UP:
// 				input.action = KEY_ACTION_UP;
// 				return input;
// 			case KEY_DOWN:
// 				input.action = KEY_ACTION_DOWN;
// 				return input;
// 			case KEY_LEFT:
// 				input.action = KEY_ACTION_LEFT;
// 				return input;
// 			case KEY_RIGHT:
// 				input.action = KEY_ACTION_RIGHT;
// 				return input;
// 			case KEY_HOME:
// 				input.action = KEY_ACTION_HOME;
// 				return input;
// 			case KEY_END:
// 				input.action = KEY_ACTION_END;
// 				return input;
// 			case KEY_INSERT:
// 				input.action = KEY_ACTION_INSERT;
// 				return input;
// 			case KEY_DELETE:
// 				input.action = KEY_ACTION_DELETE;
// 				return input;
// 			case KEY_PAGEUP:
// 				input.action = KEY_ACTION_PAGE_UP;
// 				return input;
// 			case KEY_PAGEDOWN:
// 				input.action = KEY_ACTION_PAGE_DOWN;
// 				return input;
// 			default:
// 				// Unknown extended key
// 				continue;
// 			}
// 		}

// 		// Try to get a character from keymap
// 		char c = keymap_lookup_char(evt.id.scancode, evt.id.extended,
// 					    evt.is_shift, evt.is_caps_lock);

// 		if (c != 0)
// 		{
// 			// Check if it's a special character that needs special handling
// 			if (c == '\b')
// 			{
// 				input.type = KEY_TYPE_SPECIAL;
// 				input.action = KEY_ACTION_BACKSPACE;
// 				return input;
// 			}
// 			if (c == '\n')
// 			{
// 				input.type = KEY_TYPE_SPECIAL;
// 				input.action = KEY_ACTION_ENTER;
// 				return input;
// 			}
// 			if (c == '\t')
// 			{
// 				input.type = KEY_TYPE_SPECIAL;
// 				input.action = KEY_ACTION_TAB;
// 				return input;
// 			}
// 			if (c == 27)
// 			{ // ESC
// 				input.type = KEY_TYPE_SPECIAL;
// 				input.action = KEY_ACTION_ESC;
// 				return input;
// 			}

// 			// Regular printable character
// 			input.type = KEY_TYPE_CHAR;
// 			input.character = c;
// 			return input;
// 		}

// 		// Unknown key, continue waiting
// 	}
// }

#include <drivers/tty/tty.h>
#include <drivers/tty/keyboard.h>
#include <drivers/tty/keymap.h>

#include <hubble/input.h>
#include <hubble/init.h>

/*
 * Стан модифікаторів — відновлюємо з input_raw_event_t.
 * input core передає кожну клавішу окремо, тому треба
 * відстежувати стан shift/ctrl/alt/caps самостійно.
 */
typedef struct
{
	bool shift;
	bool ctrl;
	bool alt;
	bool caps_lock;
} kbd_state_t;

static kbd_state_t kbd_state = {0};

/* ── Розпізнати модифікатор за code ──────────────────────────────────────── */

static void update_modifiers(uint16_t code, bool pressed)
{
	/* code = scancode | 0x100 якщо extended (дивись keyboard.c) */
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

/* ── Головна функція обробки події ───────────────────────────────────────── */

static void tty_handle_key(uint16_t code, bool pressed)
{
	uint8_t sc = code & 0xFF;
	bool extended = code & 0x100;

	/* --- CTRL+C, CTRL+D, CTRL+L --- */
	if (kbd_state.ctrl && !extended)
	{
		if (!pressed)
			return;
		switch (sc)
		{
		case KEY_C:
			tty_input_char(tty_current, 0x03);
			return; /* ETX */
		case KEY_D:
			tty_input_char(tty_current, 0x04);
			return; /* EOT */
		case KEY_L:
			tty_input_char(tty_current, '\f');
			return; /* FF  */
		}
	}

	if (!pressed)
		return; /* решту release ігноруємо */

	/* --- Стрілки та навігація (extended) --- */
	if (extended)
	{
		/* tty в canonical режимі генерує VT100 escape sequences */
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

	/* --- Функціональні клавіші --- */
	if (sc >= KEY_F1 && sc <= KEY_F10)
	{
		/* VT100: F1=\x1bOP, F2=\x1bOQ, ... — поки просто ігноруємо */
		return;
	}

	/* --- Звичайний символ через keymap --- */
	char c = keymap_lookup_char(sc, extended, kbd_state.shift, kbd_state.caps_lock);
	if (c != 0)
		tty_input_char(tty_current, c);
}

/* ── input_handler ───────────────────────────────────────────────────────── */

static bool tty_kbd_match(input_handler_t *handler, input_dev_t *dev)
{
	(void)handler;
	return input_test_bit(EV_KEY, dev->evbit);
}

static input_handle_t tty_kbd_handle; /* статичний — один tty, одна клава */

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

	bool pressed = ev->value == 1; /* 1=press, 0=release, 2=repeat */
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

/* ── initcall ────────────────────────────────────────────────────────────── */

static int tty_keyboard_initcall(void)
{
	input_register_handler(&tty_kbd_handler);
	return 0;
}

device_initcall(tty_keyboard_initcall);
