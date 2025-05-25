// kernel/apps/os.cpp
#include "utils/framebuffer.h"
#include "utils/color.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

void handle_neofetch();

class Screen
{
private:
	framebuffer_info_t *fb;

public:
	Screen(framebuffer_info_t *fb) : fb(fb) {}
	~Screen() {}

	void drawPixel(int x, int y, uint32_t color)
	{
		auto pixel_ptr = (uint32_t *)fb->base;
		pixel_ptr[y * fb->width + x] = color;
	}

	int getWidth() const { return fb->width; }
	int getHeight() const { return fb->height; }
};

extern "C" void os_main(framebuffer_info_t *fb)
{
	Screen screen(fb);

	for (int i = 0; i < fb->width * fb->height; i++)
		screen.drawPixel(i % fb->width, i / fb->width, rgb(0, 0, 0));
	handle_neofetch();

	int x;
	printf("Enter x: ");
	scanf("%d", &x);
	printf("x: %d\n", x);
}

void handle_neofetch()
{
	printf(
		" _    _         _      _      _        \n"
		"| |  | |       | |    | |    | |       \n"
		"| |__| | _   _ | |__  | |__  | |  ___  \n"
		"|  __  || | | || '_ \\ | '_ \\ | | / _ \\ \n"
		"| |  | || |_| || |_) || |_) || ||  __/ \n"
		"|_|  |_| \\__,_||_.__/ |_.__/ |_| \\___| \n");
}
