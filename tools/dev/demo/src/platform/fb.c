#include "fb.h"
#include <stdlib.h>

framebuffer_info_t *fb_create(uint32_t width, uint32_t height, uint8_t bpp)
{
	framebuffer_info_t *fb = malloc(sizeof(framebuffer_info_t));
	fb->width = width;
	fb->height = height;
	fb->bpp = bpp;
	fb->pitch = width * (bpp / 8);
	fb->base = malloc(fb->pitch * height);
	return fb;
}

void fb_destroy(framebuffer_info_t *fb)
{
	free(fb->base);
	free(fb);
}
