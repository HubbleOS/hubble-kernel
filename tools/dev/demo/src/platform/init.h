/**
 * @file init.h
 * @brief  Initialization and shutdown functions
 */
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

/**
 * @brief Initialize demo environment.
 *
 * Creates SDL window, renderer, texture and framebuffer.
 * Also initializes screen subsystem, mouse state and kernel tasks.
 *
 * @param w Framebuffer width
 * @param h Framebuffer height
 * @param bpp Bits per pixel
 *
 * @return demo_ctx_t Initialized demo context
 */
demo_ctx_t demo_init(uint32_t w, uint32_t h, uint8_t bpp);

/**
 * @brief Shutdown demo environment.
 *
 * Frees framebuffer and destroys SDL resources.
 *
 * @param ctx Demo context
 */
void demo_shutdown(demo_ctx_t *ctx);
