#include <stddef.h>
#include <ctype.h>
#include <sys/keyboard.h>
#include <sys/keymap.h>
#include <lib/misc.k.h>
#include <utils/font.h>
#include <io.h>

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

input_event_t keyboard_get_input(void)
{
	while (true)
	{
		key_event_t evt = keyboard_get_event();

		// input_event_t input = {0};

		input_event_t input = {
		    .type = KEY_TYPE_UNKNOWN,
		    .shift = evt.is_shift,
		    .ctrl = evt.is_ctrl,
		    .alt = evt.is_alt,
		    .character = 0,
		    .action = KEY_ACTION_NONE};

		// Skip modifier keys themselves
		if (evt.id.scancode == KEY_LEFT_SHIFT ||
		    evt.id.scancode == KEY_RIGHT_SHIFT ||
		    evt.id.scancode == KEY_LEFT_CTRL ||
		    (evt.id.scancode == KEY_RIGHT_CTRL && evt.id.extended) ||
		    evt.id.scancode == KEY_LEFT_ALT ||
		    (evt.id.scancode == KEY_RIGHT_ALT && evt.id.extended) ||
		    evt.id.scancode == KEY_CAPS_LOCK)
		{
			input.type = KEY_TYPE_MODIFIER;
			continue;
		}

		// Handle function keys
		if (!evt.id.extended && evt.id.scancode >= KEY_F1 && evt.id.scancode <= KEY_F12)
		{
			input.type = KEY_TYPE_FUNCTION;
			if (evt.id.scancode <= KEY_F10)
				input.function_key = evt.id.scancode - KEY_F1 + 1;
			else
				input.function_key = evt.id.scancode - KEY_F11 + 11;
			return input;
		}

		// Handle extended special keys (arrows, navigation)
		if (evt.id.extended)
		{
			input.type = KEY_TYPE_SPECIAL;

			switch (evt.id.scancode)
			{
			case KEY_UP:
				input.action = KEY_ACTION_UP;
				return input;
			case KEY_DOWN:
				input.action = KEY_ACTION_DOWN;
				return input;
			case KEY_LEFT:
				input.action = KEY_ACTION_LEFT;
				return input;
			case KEY_RIGHT:
				input.action = KEY_ACTION_RIGHT;
				return input;
			case KEY_HOME:
				input.action = KEY_ACTION_HOME;
				return input;
			case KEY_END:
				input.action = KEY_ACTION_END;
				return input;
			case KEY_INSERT:
				input.action = KEY_ACTION_INSERT;
				return input;
			case KEY_DELETE:
				input.action = KEY_ACTION_DELETE;
				return input;
			case KEY_PAGEUP:
				input.action = KEY_ACTION_PAGE_UP;
				return input;
			case KEY_PAGEDOWN:
				input.action = KEY_ACTION_PAGE_DOWN;
				return input;
			default:
				// Unknown extended key
				continue;
			}
		}

		// Try to get a character from keymap
		char c = keymap_lookup_char(evt.id.scancode, evt.id.extended,
					    evt.is_shift, evt.is_caps_lock);

		if (c != 0)
		{
			// Check if it's a special character that needs special handling
			if (c == '\b')
			{
				input.type = KEY_TYPE_SPECIAL;
				input.action = KEY_ACTION_BACKSPACE;
				return input;
			}
			if (c == '\n')
			{
				input.type = KEY_TYPE_SPECIAL;
				input.action = KEY_ACTION_ENTER;
				return input;
			}
			if (c == '\t')
			{
				input.type = KEY_TYPE_SPECIAL;
				input.action = KEY_ACTION_TAB;
				return input;
			}
			if (c == 27)
			{ // ESC
				input.type = KEY_TYPE_SPECIAL;
				input.action = KEY_ACTION_ESC;
				return input;
			}

			// Regular printable character
			input.type = KEY_TYPE_CHAR;
			input.character = c;
			return input;
		}

		// Unknown key, continue waiting
	}
}

#define KBD_BUFFER_SIZE 128

static key_event_t kbd_buffer[KBD_BUFFER_SIZE];
static volatile size_t kbd_head = 0;
static volatile size_t kbd_tail = 0;

static void kbd_push(key_event_t e)
{
	size_t next = (kbd_head + 1) % KBD_BUFFER_SIZE;
	if (next != kbd_tail)
	{
		kbd_buffer[kbd_head] = e;
		kbd_head = next;
	}
}

static bool kbd_pop(key_event_t *out)
{
	if (kbd_tail == kbd_head)
		return false;
	*out = kbd_buffer[kbd_tail];
	kbd_tail = (kbd_tail + 1) % KBD_BUFFER_SIZE;
	return true;
}

static bool process_scancode_once(uint8_t raw, key_event_t *out_evt)
{
	static bool extended = false;
	static bool shift_pressed = false;
	static bool ctrl_pressed = false;
	static bool alt_pressed = false;
	static bool caps_lock_active = false;

	uint8_t scancode = GET_SCANCODE(raw);
	for (int i = 0; i < 8; i++)
	{
		outb(0x3f8, ((raw >> i) & 1) + '0');
	}
	outb(0x3f8, '\n');

	bool released = IS_RELEASED(raw);

	if (IS_EXTENDED(raw))
	{
		// mark that next scancode is extended; don't emit event yet
		extended = true;
		return false;
	}

	// handle modifiers
	if (scancode == KEY_LEFT_SHIFT || scancode == KEY_RIGHT_SHIFT)
	{
		shift_pressed = !released;
	}
	else if ((scancode == KEY_LEFT_CTRL && !extended) || (scancode == KEY_RIGHT_CTRL && extended))
	{
		ctrl_pressed = !released;
	}
	else if ((scancode == KEY_LEFT_ALT && !extended) || (scancode == KEY_RIGHT_ALT && extended))
	{
		alt_pressed = !released;
	}
	else if (scancode == KEY_CAPS_LOCK && !released)
	{
		caps_lock_active = !caps_lock_active;
	}

	// prepare event
	out_evt->id.scancode = scancode;
	out_evt->id.extended = extended;
	out_evt->released = released;
	out_evt->is_shift = shift_pressed;
	out_evt->is_ctrl = ctrl_pressed;
	out_evt->is_alt = alt_pressed;
	out_evt->is_caps_lock = caps_lock_active;

	// reset extended flag after consuming
	extended = false;
	return true;
}

#include <utils/font.h>

// для IRQ — читаем без ожидания
static inline uint8_t kbd_read_scancode_irq(void)
{
	// extern uint8_t inb(uint16_t port);
	return inb(0x60);
}

void keyboard_irq(registers_t *r)
{
	outb(0x3f8, 'K');
	uint8_t status = inb(0x64);

	if ((status & 0x20))
		return;
	if (!(status & 0x01))
		return;

	uint8_t raw = kbd_read_scancode_irq();

	outb(0x3f8, 'O');
	for (size_t i = 0; i < 8; i++)
	{
		outb(0x3f8, ((raw >> i) & 1) + '0');
	}

	outb(0x3f8, '\n');

	// debug: печатаем scancode — поможет понять, приходят ли IRQ
	// printk("[kbd irq] raw=0x%02x\n", raw);

	key_event_t evt;
	if (process_scancode_once(raw, &evt))
	{
		// outb(0x3f8, 'P');
		kbd_push(evt);
	}
	// outb(0x3f8, 'E');
	// не отправляем EOI здесь — это делает общий irq_handler после возврата
}

char keyboard_get_char(void)
{
	key_event_t ev;

	while (true)
	{
		// Атомарно проверяем буфер
		asm volatile("cli");
		bool has_event = kbd_pop(&ev);
		asm volatile("sti");

		if (has_event)
		{
			if (ev.released)
				continue;

			return keymap_lookup_char(ev.id.scancode, ev.id.extended,
						  ev.is_shift, ev.is_caps_lock);
		}

		// Буфер пуст - ждём прерывания
		asm volatile("hlt");
	}
}

key_event_t keyboard_get_event(void)
{
	key_event_t ev;

	while (true)
	{
		// Атомарно проверяем буфер
		asm volatile("cli");
		bool has_event = kbd_pop(&ev);
		asm volatile("sti"); // ВСЕГДА включаем обратно!

		if (has_event && !ev.released)
			return ev;

		// Ждём следующего прерывания
		asm volatile("hlt");
	}
}

#include <dev/ps2.h>
#include <printk.h>

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

void keyboard_init()
{
	__asm__ volatile("cli");

	uint8_t act;
	keyboard_write(0xF0);
	act = keyboard_read();

	printk("Keyboard active: %02x\n", act);

	keyboard_write(0x01);
	act = keyboard_read();
	printk("Keyboard active: %02x\n", act);

	__asm__ volatile("sti");
	printk("Keyboard initialized\n");
}
