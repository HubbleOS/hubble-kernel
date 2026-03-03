#pragma once
#include <stdint.h>
#include <stddef.h>

#include <bootinfo/framebuffer.h>
#include "utils/color.h"

typedef struct
{
	framebuffer_info_t *fb;

	int (*getWidth)(void *self);
	int (*getHeight)(void *self);
	int (*drawPixel)(void *self, int x, int y, color_t color);
	int (*drawRect)(void *self, int x, int y, int w, int h, color_t color);
} screen_t;

extern screen_t screen;

extern uint32_t *framebuffer_back;
extern int fb_width;
extern int fb_height;

void screen_init(framebuffer_info_t *fb);
void screen_present(void);

color_t color_blend(color_t src, color_t dst);
