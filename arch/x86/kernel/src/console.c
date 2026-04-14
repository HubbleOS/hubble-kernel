#include "textout.h"

#include <hubble/string.h>
#include <hubble/printk.h>
#include <hubble/color.h>
#include <hubble/fb.h>

#include <io.h>

#define CHAR_WIDTH 8
#define CHAR_HEIGHT 8

static int early_x = 0;
static int early_y = 0;
static framebuffer_info_t *early_fb = NULL;

void early_putchar(char c)
{
	// serial
	outb(0x3f8, c);

	if (!early_fb || !early_fb->base)
		return;

	if (c == '\n')
	{
		early_x = 0;
		early_y += CHAR_HEIGHT;

		if (early_y + CHAR_HEIGHT > (int)early_fb->height)
		{
			size_t line_size = early_fb->pitch;
			uint8_t *fb_base = (uint8_t *)early_fb->base;

			for (int y = 0; y < (int)early_fb->height - CHAR_HEIGHT; y++)
				memcpy(fb_base + y * line_size,
				       fb_base + (y + CHAR_HEIGHT) * line_size,
				       line_size);

			for (int y = (int)early_fb->height - CHAR_HEIGHT; y < (int)early_fb->height; y++)
				memset(fb_base + y * line_size, 0, line_size);

			early_y = early_fb->height - CHAR_HEIGHT;
		}
		return;
	}

	if (c == '\r')
	{
		early_x = 0;
		return;
	}

	if (c == '\b')
	{
		if (early_x >= CHAR_WIDTH)
			early_x -= CHAR_WIDTH;
		return;
	}

	if (c == '\t')
	{
		for (int i = 0; i < 4; i++)
			early_putchar(' ');
		return;
	}

	if (early_x + CHAR_WIDTH > (int)early_fb->width)
		early_putchar('\n');

	draw_char(c, early_x, early_y, CHAR_WIDTH, CHAR_HEIGHT, COLOR_WHITE);
	early_x += CHAR_WIDTH;
}

void printk_init(framebuffer_info_t *fb)
{
	early_fb = fb;
	early_x = 0;
	early_y = 0;
	printk_set_output(early_putchar);
}
