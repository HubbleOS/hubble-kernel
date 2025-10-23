#include <stddef.h>

#include <sys/keyboard.h>
#include <sys/keymap.h>

#include <utils/font.h>

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
    {.id = {KEY_RIGHT_CTRL, false}, 0, 0},
    {.id = {KEY_LEFT_ALT, false}, 0, 0},
    {.id = {KEY_RIGHT_ALT, true}, 0, 0},

    {.id = {KEY_LEFT, true}, '<', '<'},
    {.id = {KEY_RIGHT, true}, '>', '>'},
    {.id = {KEY_UP, true}, '^', '^'},
    {.id = {KEY_DOWN, true}, 'v', 'v'},
};

const size_t keymap_size = sizeof(keymap) / sizeof(keymap[0]);

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

key_event_t read_key_event()
{
	static bool extended = false;
	static bool shift_pressed = false;
	static bool ctrl_pressed = false;
	static bool alt_pressed = false;
	static bool caps_lock_active = false;

	while (1)
	{
		uint8_t sc = kbd_read_scancode();
		uint8_t scancode = GET_SCANCODE(sc);
		bool released = IS_RELEASED(sc);

		if (IS_EXTENDED(sc))
		{
			extended = true;
			continue;
		}

		// Shift (left or right)
		if (scancode == KEY_LEFT_SHIFT || scancode == KEY_RIGHT_SHIFT)
		{
			shift_pressed = !released;
		}
		// Ctrl (left or right)
		else if ((scancode == KEY_LEFT_CTRL && !extended) || (scancode == KEY_RIGHT_CTRL && extended))
		{
			ctrl_pressed = !released;
		}
		// Alt (left or right)
		else if ((scancode == KEY_LEFT_ALT && !extended) || (scancode == KEY_RIGHT_ALT && extended))
		{
			alt_pressed = !released;
		}
		// Caps Lock toggle
		else if (scancode == KEY_CAPS_LOCK && !released)
		{
			caps_lock_active = !caps_lock_active;
		}

		key_event_t evt = {
		    .id.scancode = scancode,
		    .id.extended = extended,
		    .released = released,
		    .is_shift = shift_pressed,
		    .is_ctrl = ctrl_pressed,
		    .is_alt = alt_pressed,
		    .is_caps_lock = caps_lock_active};

		extended = false;
		return evt;
	}
}
