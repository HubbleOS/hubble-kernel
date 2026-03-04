#pragma once

#include <_cheader.h>
#include <stdint.h>

extern const uint8_t font_glyph[256][8];

_Begin_C_Header;

uint8_t *get_glyph(char c);

_End_C_Header;
