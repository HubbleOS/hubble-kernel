#pragma once

#include "utils/framebuffer.h"
#include "utils/color.h"

class Screen
{
private:
	framebuffer_info_t *fb;

public:
	Screen(framebuffer_info_t *fb);
	~Screen();

	void drawPixel(int x, int y, color_t color);
	int getWidth() const;
	int getHeight() const;
	void clearScreen();

	framebuffer_info_t *getFramebuffer();
};
