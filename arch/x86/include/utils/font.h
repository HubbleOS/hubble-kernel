#pragma once
#include <stdint.h>
#include "framebuffer.h"

#define CHAR_WIDTH 8
#define CHAR_HEIGHT 8

// Шрифт для символів 'A' до 'Z'
extern const uint8_t font[131][8];

// Вивід одного символу (тільки великі латинські літери)
void draw_char(framebuffer_info_t* fb, char c, int x, int y, uint32_t color, ...);


// Вивід рядка тексту
void print_text(framebuffer_info_t* fb, const char* str, int x, int y, uint32_t color);
