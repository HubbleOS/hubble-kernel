#include "sdl_render.h"

void sdl_render(framebuffer_info_t *fb, SDL_Renderer *renderer, SDL_Texture *texture)
{
	SDL_UpdateTexture(texture, NULL, fb->base, fb->pitch);
	SDL_RenderCopy(renderer, texture, NULL, NULL);
	SDL_RenderPresent(renderer);
}
