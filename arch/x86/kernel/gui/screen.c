#include "screen.h"
#include <stdlib.h>
#include <string.h>

framebuffer_info_t *g_fb = NULL;
uint32_t *framebuffer_back = NULL;
int fb_width = 0;
int fb_height = 0;

void screen_present(void)
{
	if (!g_fb || !framebuffer_back)
		return;

	uint8_t *dst_base = (uint8_t *)g_fb->base;

	for (int row = 0; row < fb_height; row++)
	{
		uint32_t *dst = (uint32_t *)(dst_base + row * g_fb->pitch);
		uint32_t *src = framebuffer_back + row * fb_width;

		for (int col = 0; col < fb_width; col++)
			dst[col] = src[col];
	}
}

void screen_present_rect(int x, int y, int w, int h)
{
	if (!g_fb || !framebuffer_back)
		return;

	// crop to the edges of the screen
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
		uint32_t *dst = (uint32_t *)(dst_base + row * g_fb->pitch + x * sizeof(uint32_t));
		uint32_t *src = framebuffer_back + row * fb_width + x;

		for (int col = 0; col < w; col++)
			dst[col] = src[col];
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
