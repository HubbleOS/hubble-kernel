#pragma once

#include <stdint.h>

extern const uint8_t font_glyph[256][8];

uint8_t *get_glyph(char c);
