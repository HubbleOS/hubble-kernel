#pragma once
#include <_cheader.h>
#include <stdint.h>
#include <io.h>
#include <bootinfo/framebuffer.h>

typedef uint32_t color_t;

#define CHAR_WIDTH 8
#define CHAR_HEIGHT 8

_Begin_C_Header;

void draw_char(framebuffer_info_t *fb, char c, int x, int y, int w, int h, color_t font_color);

_End_C_Header;
