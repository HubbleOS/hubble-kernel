#pragma once
#include <stdint.h>
#include <utils/color/color.h>

void draw_rounded_rect(uint32_t *buffer, int buf_w, int buf_h, int x, int y,
                       int w, int h, int radius, int feather, color_t color);
