#include "events.h"
#include "mouse.h"

extern mouse_t *mouse;

/**
 * @brief Process all SDL events.
 *
 * Internally polls SDL event queue using SDL_PollEvent().
 * Mouse coordinates are copied into the global mouse structure
 * used by the GUI subsystem.
 *
 * The function exits early if SDL_QUIT event is received.
 */

int events_process(void)
{
	SDL_Event e;

	while (SDL_PollEvent(&e))
	{
		if (e.type == SDL_QUIT)
			return 0;

		if (e.type == SDL_MOUSEMOTION)
		{
			mouse->x = e.motion.x;
			mouse->y = e.motion.y;
		}

		if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT)
			mouse->left = 1;

		if (e.type == SDL_MOUSEBUTTONUP && e.button.button == SDL_BUTTON_LEFT)
			mouse->left = 0;
	}

	return 1;
}
