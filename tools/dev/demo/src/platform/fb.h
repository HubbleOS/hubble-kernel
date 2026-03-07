#pragma once
#include <stdint.h>
// #include <bootinfo/framebuffer.h>

#include <stdint.h>

typedef struct
{
	uint32_t *base;
	uint32_t width;
	uint32_t height;
	uint32_t pitch; // bytes per row
	uint8_t bpp;	// bits per pixel
} framebuffer_info_t;

framebuffer_info_t *fb_create(uint32_t width, uint32_t height, uint8_t bpp);
void fb_destroy(framebuffer_info_t *fb);
