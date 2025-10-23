#pragma once

#include "../screen/screen.h"

class Window
{
private:
	Screen &screen;	   // Ссылка на экран
	int x, y;	   // Позиция окна (левый верхний угол)
	int width, height; // Размеры окна
	uint32_t bg_color; // Цвет фона окна

public:
	Window(Screen &screen, int x, int y, int width, int height, uint32_t bg_color = 0x000000)
	    : screen(screen), x(x), y(y), width(width), height(height), bg_color(bg_color) {}

	void drawPixel(int px, int py, uint32_t color)
	{
		if (px < 0 || px >= width || py < 0 || py >= height)
			return; // За пределами окна
		screen.drawPixel(x + px, y + py, color);
	}

	void clear()
	{
		for (int py = 0; py < height; py++)
			for (int px = 0; px < width; px++)
				drawPixel(px, py, bg_color);
	}

	Screen *getScreen() { return &screen; }

	int getWidth() const { return width; }
	int getHeight() const { return height; }

	int getX() const { return x; }
	int getY() const { return y; }
	uint32_t getBgColor() const { return bg_color; }
};
