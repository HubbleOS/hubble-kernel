#pragma once
#include <stdint.h>
#include "framebuffer.h"
#include "utils/color.h"

#define CHAR_WIDTH 8
#define CHAR_HEIGHT 8

extern const uint8_t font[256][8];

#include "_cheader.h"
_Begin_C_Header;

void draw_char(framebuffer_info_t *fb, char c, int x, int y, int w, int h, color font_color);
void clear_char_area(framebuffer_info_t *fb, int x, int y, int w, int h, color bg_color);

_End_C_Header;

#define KBD_DATA_PORT 0x60
#define KBD_STATUS_PORT 0x64
#define KBD_OBF 0x01 // Output buffer full

_Begin_C_Header;
static inline uint8_t inb(uint16_t port)
{
	uint8_t val;
	__asm__ volatile("inb %1, %0" : "=a"(val) : "Nd"(port));
	return val;
}

static inline uint8_t kbd_read_scancode(void)
{
	while (!(inb(KBD_STATUS_PORT) & KBD_OBF))
		;
	return inb(KBD_DATA_PORT);
}
_End_C_Header;
