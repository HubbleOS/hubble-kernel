#include "keyboard.h"
#include <SDL2/SDL.h>

// простой глобальный state модификаторов
static bool shift_pressed = false;
static bool ctrl_pressed = false;
static bool alt_pressed = false;
static bool caps_lock_active = false;

bool keyboard_poll_sdl(input_event_t *input)
{
	SDL_Event e;

	// Poll только один event за раз
	while (SDL_PollEvent(&e))
	{
		if (e.type == SDL_KEYDOWN || e.type == SDL_KEYUP)
		{
			bool pressed = (e.type == SDL_KEYDOWN);
			SDL_Keycode key = e.key.keysym.sym;

			// модификаторы
			switch (key)
			{
			case SDLK_LSHIFT:
			case SDLK_RSHIFT:
				shift_pressed = pressed;
				continue; // не отдаём модификаторы как событие
			case SDLK_LCTRL:
			case SDLK_RCTRL:
				ctrl_pressed = pressed;
				continue;
			case SDLK_LALT:
			case SDLK_RALT:
				alt_pressed = pressed;
				continue;
			case SDLK_CAPSLOCK:
				if (pressed)
					caps_lock_active = !caps_lock_active;
				continue;
			}

			if (!pressed)
				continue; // игнорируем keyup

			// формируем input_event_t
			input->shift = shift_pressed;
			input->ctrl = ctrl_pressed;
			input->alt = alt_pressed;

			char c = keymap_lookup_sdl(e.key.keysym, shift_pressed, caps_lock_active);

			if (c == '\b')
			{
				input->type = KEY_TYPE_SPECIAL;
				input->action = KEY_ACTION_BACKSPACE;
				return true;
			}
			if (c == '\n')
			{
				input->type = KEY_TYPE_SPECIAL;
				input->action = KEY_ACTION_ENTER;
				return true;
			}
			if (c == '\t')
			{
				input->type = KEY_TYPE_SPECIAL;
				input->action = KEY_ACTION_TAB;
				return true;
			}
			if (c)
			{
				input->type = KEY_TYPE_CHAR;
				input->character = c;
				return true;
			}
		}
	}

	return false; // ничего нет
}

// -------------------------------------
// простой mapping SDL_Keycode -> char
// -------------------------------------
char keymap_lookup_sdl(SDL_Keysym keysym, bool shift, bool caps)
{
	char c = 0;

	if (keysym.sym >= SDLK_a && keysym.sym <= SDLK_z)
	{
		c = 'a' + (keysym.sym - SDLK_a);
		if ((shift && !caps) || (!shift && caps))
			c = c - 'a' + 'A';
	}
	else if (keysym.sym >= SDLK_0 && keysym.sym <= SDLK_9)
	{
		c = '0' + (keysym.sym - SDLK_0);
		if (shift)
		{
			static const char shifted[] = ")!@#$%^&*(";
			c = shifted[c - '0'];
		}
	}
	else
	{
		switch (keysym.sym)
		{
		case SDLK_SPACE:
			c = ' ';
			break;
		case SDLK_RETURN:
			c = '\n';
			break;
		case SDLK_TAB:
			c = '\t';
			break;
		case SDLK_BACKSPACE:
			c = '\b';
			break;
		case SDLK_ESCAPE:
			c = 27;
			break;
		case SDLK_MINUS:
			c = shift ? '_' : '-';
			break;
		case SDLK_EQUALS:
			c = shift ? '+' : '=';
			break;
		case SDLK_LEFTBRACKET:
			c = shift ? '{' : '[';
			break;
		case SDLK_RIGHTBRACKET:
			c = shift ? '}' : ']';
			break;
		case SDLK_SEMICOLON:
			c = shift ? ':' : ';';
			break;
		case SDLK_QUOTE:
			c = shift ? '\"' : '\'';
			break;
		case SDLK_COMMA:
			c = shift ? '<' : ',';
			break;
		case SDLK_PERIOD:
			c = shift ? '>' : '.';
			break;
		case SDLK_SLASH:
			c = shift ? '?' : '/';
			break;
		case SDLK_BACKSLASH:
			c = shift ? '|' : '\\';
			break;
			// case SDLK_GRAVE:
			// 	c = shift ? '~' : '`';
			// 	break;
		}
	}

	return c;
}
