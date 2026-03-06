#pragma once

#include "../element.h"

typedef element_t button_t;

button_t *button_create(int x, int y, int w, int h, const char *text);
