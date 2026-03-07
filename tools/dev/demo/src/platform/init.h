#pragma once
#include <SDL2/SDL.h>
// #include <bootinfo/framebuffer.h>

#include "fb.h"

typedef struct
{
	SDL_Window *window;
	SDL_Renderer *renderer;
	SDL_Texture *texture;
	framebuffer_info_t *fb;
} demo_ctx_t;

demo_ctx_t demo_init(uint32_t w, uint32_t h, uint8_t bpp);
void demo_shutdown(demo_ctx_t *ctx);
