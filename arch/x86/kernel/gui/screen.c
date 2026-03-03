#include "screen.h"
#include <stdlib.h>
#include <string.h>

screen_t screen;
uint32_t *framebuffer_back = NULL;
int fb_width = 0;
int fb_height = 0;

static int screen_getWidth(void *self) { return fb_width; }
static int screen_getHeight(void *self) { return fb_height; }

static int screen_drawPixel(void *self, int x, int y, color_t color)
{
	if (x < 0 || y < 0 || x >= fb_width || y >= fb_height)
		return -1;

	uint32_t *pixel = framebuffer_back + y * fb_width + x;
	*pixel = color_blend(color, *pixel);
	return 0;
}

static int screen_drawRect(void *self, int x, int y, int w, int h, color_t color)
{
	if (w <= 0 || h <= 0)
		return -1;

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
		return -1;

	uint8_t alpha = get_alpha(color);

	for (int row = 0; row < h; row++)
	{
		uint32_t *pixel = framebuffer_back + (y + row) * fb_width + x;

		for (int col = 0; col < w; col++)
		{
			if (alpha == 255)
				pixel[col] = color;
			else
				pixel[col] = color_blend(color, pixel[col]);
		}
	}

	return 0;
}

void screen_present(void)
{
	if (!screen.fb || !framebuffer_back)
		return;

	uint8_t *dst_base = (uint8_t *)screen.fb->base;

	for (int row = 0; row < fb_height; row++)
	{
		uint32_t *dst = (uint32_t *)(dst_base + row * screen.fb->pitch);
		uint32_t *src = framebuffer_back + row * fb_width;

		for (int col = 0; col < fb_width; col++)
			dst[col] = src[col];
	}
}

void screen_init(framebuffer_info_t *fb)
{
	screen.fb = fb;
	screen.getWidth = screen_getWidth;
	screen.getHeight = screen_getHeight;
	screen.drawPixel = screen_drawPixel;
	screen.drawRect = screen_drawRect;

	fb_width = fb->width;
	fb_height = fb->height;

	framebuffer_back = malloc(fb_width * fb_height * sizeof(uint32_t));

	if (!framebuffer_back)
		return;

	for (int i = 0; i < fb_width * fb_height; i++)
		framebuffer_back[i] = 0;
}
