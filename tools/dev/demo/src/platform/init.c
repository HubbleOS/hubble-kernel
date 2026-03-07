#include "init.h"
#include "fb.h"
#include "mouse.h"

#include <stdlib.h>
#include <gui/core/screen/screen.h>
// #include <gui/ui/mouse/mouse.h>
#include <gui/utils/color/color.h>
#include <tasks/task.h>

extern mouse_t *mouse;

demo_ctx_t demo_init(uint32_t w, uint32_t h, uint8_t bpp)
{
	demo_ctx_t ctx = {0};

	SDL_Init(SDL_INIT_VIDEO);

	ctx.window = SDL_CreateWindow(
	    "Kernel FB Simulator",
	    SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
	    w, h, 0);

	ctx.renderer = SDL_CreateRenderer(ctx.window, -1,
					  SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

	ctx.texture = SDL_CreateTexture(
	    ctx.renderer,
	    SDL_PIXELFORMAT_ARGB8888,
	    SDL_TEXTUREACCESS_STREAMING,
	    w, h);

	ctx.fb = fb_create(w, h, bpp);
	screen_init(ctx.fb);

	mouse = malloc(sizeof(mouse_t));
	mouse->x = w / 2;
	mouse->y = h / 2;

	tasks_init();
	kmain_thread();

	return ctx;
}

void demo_shutdown(demo_ctx_t *ctx)
{
	fb_destroy(ctx->fb);
	SDL_DestroyTexture(ctx->texture);
	SDL_DestroyRenderer(ctx->renderer);
	SDL_DestroyWindow(ctx->window);
	SDL_Quit();
}
