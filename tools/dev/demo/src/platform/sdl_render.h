/**
 * @file sdl_render.h
 * @brief SDL framebuffer renderer
 */

#pragma once
#include "fb.h"
#include <SDL2/SDL.h>

/**
 * @brief Render framebuffer to SDL window.
 *
 * Copies framebuffer pixels into SDL texture
 * and presents it on screen.
 *
 * @param fb Framebuffer to render
 * @param renderer SDL renderer
 * @param texture SDL texture bound to framebuffer
 */
void sdl_render(framebuffer_info_t *fb, SDL_Renderer *renderer,
                SDL_Texture *texture);
