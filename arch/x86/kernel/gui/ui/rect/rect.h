#pragma once

#include "../element.h"

element_t *create_rect(int x, int y, int w, int h, color_t color);
void destroy_rect(element_t *el);
