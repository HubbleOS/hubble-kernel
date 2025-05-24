// // kernel/apps/os.cpp (C++)
#include "utils/framebuffer.h"
#include "utils/color.h"
#include <stdint.h>
#include <stdio.h>

class Screen
{
private:
	framebuffer_info_t *fb;

public:
	Screen(framebuffer_info_t *fb);
	~Screen();

	void drawPixel(int x, int y, uint32_t color);
};

Screen::Screen(framebuffer_info_t *fb) : fb(fb) {}
Screen::~Screen() {}

void Screen::drawPixel(int x, int y, uint32_t color)
{
	auto pixel_ptr = (uint32_t *)fb->base;
	pixel_ptr[y * fb->width + x] = color;
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

extern "C" void os_main(framebuffer_info_t *fb)
{
	Screen screen(fb);
	for (int i = 0; i < fb->width * fb->height; i++)
		screen.drawPixel(i % fb->width, i / fb->width, rgb(0, 0, 0));

	handle_neofetch();

	int x = 123;
	printf("x = %i\n", x);
	printf("x = %d\n", 1);
	printf("x = %d\n", 12);
	printf("x = %d\n", 123);
	printf("x = %d\n", 1234);
	printf("x = %d\n", 12345);
	printf("x = %d\n", -123456);
	printf("text = %d\n", "123");
	printf("BUKVA = %c\n", 'A');

	// scanf("%d", &x);
	// printf("x = %d\n", x);
	while (1)
	{
		int c = getchar();
		printf("char = %c\n", c);
	}
}
