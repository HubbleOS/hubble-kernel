#pragma once
#include <stdint.h>
#include <stddef.h>
#include <gui/utils/color/color.h>
#ifndef GUI_DEMO
// #include <bootinfo/framebuffer.h>

typedef struct
{
	void *base;
	unsigned int width;
	unsigned int height;
	unsigned int pitch;
	unsigned int bpp;
} framebuffer_info_t;

#else
#include "platform/fb.h"
#endif
extern framebuffer_info_t *g_fb;
extern uint32_t *framebuffer_back;
extern int fb_width;
extern int fb_height;

void screen_init(framebuffer_info_t *fb);
void screen_present_rect(int x, int y, int w, int h);
