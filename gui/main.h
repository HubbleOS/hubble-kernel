#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef struct
{
	uint32_t *base;
	uint32_t width;
	uint32_t height;
	uint32_t pitch; // bytes per row
	uint8_t bpp;	// bits per pixel
} framebuffer_info_t;

typedef struct
{
	int32_t x, y;
	bool left, right, middle; // current held state
	bool left_clicked;	  // set on press, you clear it after handling
	bool right_clicked;
} mouse_t;

mouse_t *get_mouse_info(void);

void kmain(framebuffer_info_t *fb);
