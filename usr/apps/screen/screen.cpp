#include "screen.h"

#include <stddef.h>

Screen::Screen(framebuffer_info_t *fb) : fb(fb) {}
Screen::~Screen() {}

void Screen::drawPixel(int x, int y, color_t c)
{
	color_t *pixel_ptr = (color_t *)fb->base;
	if (x < 0 || x >= fb->width || y < 0 || y >= fb->height)
		return;
	pixel_ptr[y * fb->width + x] = c;
}

int Screen::getWidth() const { return fb->width; }
int Screen::getHeight() const { return fb->height; }

void Screen::clearScreen()
{
	color_t *pixel_ptr = (color_t *)fb->base;
	size_t pixels = fb->width * fb->height;
	for (size_t i = 0; i < pixels; i++)
		pixel_ptr[i] = COLOR_BLACK;
}

framebuffer_info_t *Screen::getFramebuffer() { return fb; }