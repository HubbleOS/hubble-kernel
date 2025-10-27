#pragma once
#include <_cheader.h>
#include <stdint.h>
#include <io.h>
#include <bootinfo/framebuffer.h>
#include "utils/color.h"

#define CHAR_WIDTH 8
#define CHAR_HEIGHT 8

extern const uint8_t font[256][8];

_Begin_C_Header;

void draw_char(framebuffer_info_t *fb, char c, int x, int y, int w, int h, color_t font_color);
void clear_char_area(framebuffer_info_t *fb, int x, int y, int w, int h, color_t bg_color);

#define KBD_DATA_PORT 0x60
#define KBD_STATUS_PORT 0x64
#define KBD_OBF 0x01 // Output buffer full

static inline uint8_t kbd_read_scancode(void)
{
	while (!(inb(KBD_STATUS_PORT) & KBD_OBF))
		;
	return inb(KBD_DATA_PORT);
}

_End_C_Header;
