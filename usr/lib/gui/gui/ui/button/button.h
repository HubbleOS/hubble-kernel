#pragma once

#include <gui/core/element/element.h>

typedef element_t button_t;

button_t *button_create(int x, int y, int w, int h, const char *text);
void button_destroy(button_t *btn);
