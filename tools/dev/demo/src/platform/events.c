#include "events.h"
#include "mouse.h"
#include "keyboard.h"

#include <stdbool.h>

extern mouse_t *mouse;

int events_process(void)
{
	SDL_Event e;

	while (SDL_PollEvent(&e))
	{
		if (e.type == SDL_QUIT)
			return 0;

		//  миша
		if (e.type == SDL_MOUSEMOTION)
		{
			mouse->x = e.motion.x;
			mouse->y = e.motion.y;
		}

		if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT)
			mouse->left = 1;

		if (e.type == SDL_MOUSEBUTTONUP && e.button.button == SDL_BUTTON_LEFT)
			mouse->left = 0;

		//  клавіатура
		if (e.type == SDL_KEYDOWN)
		{
			SDL_Keycode key = e.key.keysym.sym;
			bool shift = (e.key.keysym.mod & KMOD_SHIFT) != 0;
			bool ctrl = (e.key.keysym.mod & KMOD_CTRL) != 0;
			bool alt = (e.key.keysym.mod & KMOD_ALT) != 0;

			input_event_t ev = {0};
			ev.shift = shift;
			ev.ctrl = ctrl;
			ev.alt = alt;

			switch (key)
			{
			case SDLK_BACKSPACE:
				ev.type = KEY_TYPE_SPECIAL;
				ev.action = KEY_ACTION_BACKSPACE;
				break;
			case SDLK_RETURN:
				ev.type = KEY_TYPE_SPECIAL;
				ev.action = KEY_ACTION_ENTER;
				break;
			case SDLK_TAB:
				ev.type = KEY_TYPE_SPECIAL;
				ev.action = KEY_ACTION_TAB;
				break;
			case SDLK_ESCAPE:
				ev.type = KEY_TYPE_SPECIAL;
				ev.action = KEY_ACTION_ESC;
				break;
			case SDLK_UP:
				ev.type = KEY_TYPE_SPECIAL;
				ev.action = KEY_ACTION_UP;
				break;
			case SDLK_DOWN:
				ev.type = KEY_TYPE_SPECIAL;
				ev.action = KEY_ACTION_DOWN;
				break;
			case SDLK_LEFT:
				ev.type = KEY_TYPE_SPECIAL;
				ev.action = KEY_ACTION_LEFT;
				break;
			case SDLK_RIGHT:
				ev.type = KEY_TYPE_SPECIAL;
				ev.action = KEY_ACTION_RIGHT;
				break;
			case SDLK_HOME:
				ev.type = KEY_TYPE_SPECIAL;
				ev.action = KEY_ACTION_HOME;
				break;
			case SDLK_END:
				ev.type = KEY_TYPE_SPECIAL;
				ev.action = KEY_ACTION_END;
				break;
			case SDLK_INSERT:
				ev.type = KEY_TYPE_SPECIAL;
				ev.action = KEY_ACTION_INSERT;
				break;
			case SDLK_DELETE:
				ev.type = KEY_TYPE_SPECIAL;
				ev.action = KEY_ACTION_DELETE;
				break;
			case SDLK_PAGEUP:
				ev.type = KEY_TYPE_SPECIAL;
				ev.action = KEY_ACTION_PAGE_UP;
				break;
			case SDLK_PAGEDOWN:
				ev.type = KEY_TYPE_SPECIAL;
				ev.action = KEY_ACTION_PAGE_DOWN;
				break;
			default:
				if (key >= 32 && key < 127)
				{
					char c = (char)key;
					if (shift && c >= 'a' && c <= 'z')
						c -= 32;
					ev.type = KEY_TYPE_CHAR;
					ev.character = c;
				}
				break;
			}

			if (ev.type == KEY_TYPE_CHAR || ev.type == KEY_TYPE_SPECIAL)
				keyboard_queue_push(ev);
		}
	}

	return 1;
}
