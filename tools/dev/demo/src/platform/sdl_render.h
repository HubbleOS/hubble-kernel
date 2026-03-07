#pragma once
#include <SDL2/SDL.h>
// #include <bootinfo/framebuffer.h>

#include "fb.h"

void sdl_render(framebuffer_info_t *fb, SDL_Renderer *renderer, SDL_Texture *texture);
