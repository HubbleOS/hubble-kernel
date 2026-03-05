#include "screen.h"
#include <stdlib.h>
#include <string.h>

framebuffer_info_t *g_fb = NULL;
uint32_t *framebuffer_back = NULL;
int fb_width = 0;
int fb_height = 0;

void screen_present_rect(int x, int y, int w, int h)
{
	if (!g_fb || !framebuffer_back)
		return;

	if (x < 0)
	{
		w += x;
		x = 0;
	}
	if (y < 0)
	{
		h += y;
		y = 0;
	}
	if (x + w > fb_width)
		w = fb_width - x;
	if (y + h > fb_height)
		h = fb_height - y;
	if (w <= 0 || h <= 0)
		return;

	uint8_t *dst_base = (uint8_t *)g_fb->base;

	for (int row = y; row < y + h; row++)
	{
		void *dst = dst_base + row * g_fb->pitch + x * 4;
		void *src = framebuffer_back + row * fb_width + x;
		memcpy(dst, src, w * 4);
	}
}

void screen_init(framebuffer_info_t *fb)
{
	if (!fb)
		return;

	g_fb = fb;

	fb_width = fb->width;
	fb_height = fb->height;

	framebuffer_back = malloc(fb_width * fb_height * sizeof(uint32_t));

	if (!framebuffer_back)
		return;

	for (int i = 0; i < fb_width * fb_height; i++)
		framebuffer_back[i] = 0;
}
