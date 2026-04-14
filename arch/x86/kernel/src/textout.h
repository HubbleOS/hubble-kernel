#pragma once

#include <_cheader.h>
#include <stdint.h>
#include <hubble/color.h>

#define CHAR_WIDTH 8
#define CHAR_HEIGHT 8

_Begin_C_Header;

void early_putchar(char c);

void draw_char(char c, int x, int y, int w, int h, color_t font_color);

_End_C_Header;
