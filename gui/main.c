#include <SDL2/SDL.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include "main.h"

#include <gui/core/screen/screen.h>
#include <gui/core/object/object.h>
#include <gui/core/compositor/compositor.h>

#include <gui/ui/button/button.h>
#include <gui/ui/сursor/cursor.h>
#include <gui/ui/mouse/mouse.h>
#include <gui/ui/window/window.h>
#include <gui/ui/rect/rect.h>
#include <gui/ui/text/text.h>
#include <gui/ui/canvas/canvas.h>
#include <lab/tasks.h>

#include "gui/background.h"
// Mirror your kernel's framebuffer struct exactly

int fb_width = 0;
int fb_height = 0;
uint32_t *framebuffer_back = NULL;

framebuffer_info_t *fb_create(uint32_t width, uint32_t height, uint8_t bpp)
{
	framebuffer_info_t *fb = malloc(sizeof(framebuffer_info_t));
	fb->width = width;
	fb->height = height;
	fb->bpp = bpp;
	fb->pitch = width * (bpp / 8); // bytes per row — exactly like real hw
	fb->base = malloc(fb->pitch * height);
	return fb;
}

mouse_t *mouse_g = NULL;
cursor_t *cursor = NULL;

// static double_buffer_t g_db = {0};
// static framebuffer_info_t *g_fb = NULL;

// void db_init(uint32_t width, uint32_t height)
// {
// 	g_db.width = width;
// 	g_db.height = height;
// 	g_db.pitch = width * 4;
// 	g_db.front = calloc(height, width * 4);
// 	g_db.back = calloc(height, width * 4);
// 	atomic_store(&g_db.dirty, 0);
// 	atomic_store(&g_db.running, 1);
// 	pthread_mutex_init(&g_db.swap_mutex, NULL);
// }

void SDL_Render(framebuffer_info_t *fb, SDL_Renderer *renderer, SDL_Texture *texture)
{

	SDL_UpdateTexture(texture, NULL, fb->base, fb->pitch);
	SDL_RenderCopy(renderer, texture, NULL, NULL);
	SDL_RenderPresent(renderer);
}

uint32_t width = 1920;
uint32_t height = 1080;
uint8_t bpp = 32;

int main()
{
	SDL_Init(SDL_INIT_VIDEO);

	framebuffer_info_t *fb = fb_create(width, height, bpp);

	SDL_Window *window = SDL_CreateWindow(
	    "Kernel FB Simulator",
	    SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
	    width, height, 0);

	SDL_Renderer *renderer = SDL_CreateRenderer(window, -1,
						    SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

	SDL_Texture *texture = SDL_CreateTexture(
	    renderer,
	    SDL_PIXELFORMAT_ARGB8888,
	    SDL_TEXTUREACCESS_STREAMING,
	    width, height);

	screen_init(fb);
	compositor_init();
	mouse_t *m = malloc(sizeof(mouse_t));
	m->x = 1920 / 2;
	m->y = 1080 / 2;

	background_create(rgb(188, 49, 49));
	cursor_t *cursor = cursor_create(16, 16, rgb(0, 0, 0), rgb(255, 255, 255));
	graph_app_init();
	geometry_app_init();
	int running = 1;
	while (running)
	{
		SDL_Event e;
		while (SDL_PollEvent(&e))
		{
			if (e.type == SDL_QUIT)
				running = 0;

			// Mouse events — same as your ps/2 mouse
			if (e.type == SDL_MOUSEMOTION)
			{
				m->x = e.motion.x;
				m->y = e.motion.y;
			}
			if (e.type == SDL_MOUSEBUTTONDOWN)
			{
				// check if presed
				if (e.button.button == SDL_BUTTON_LEFT)
					m->left = 1;
			}
			if (e.type == SDL_MOUSEBUTTONUP)
			{
				// check if relesed
				if (e.button.button == SDL_BUTTON_LEFT)
					m->left = 0;
			}

			// Keyboard
			if (e.type == SDL_KEYDOWN)
			{
				SDL_Scancode sc = e.key.keysym.scancode;
			}
		}

		kmain(fb);

		SDL_Render(fb, renderer, texture);
	}

	free(fb->base);
	free(fb);
	SDL_DestroyTexture(texture);
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	SDL_Quit();
	return 0;
}
