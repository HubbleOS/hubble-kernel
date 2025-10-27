#pragma once

#include <stdint.h>

typedef struct
{
	void *base;
	unsigned int width;
	unsigned int height;
	unsigned int pitch;
	unsigned int bpp;
} framebuffer_info_t;
