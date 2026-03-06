#pragma once

#include <gui/core/element/element.h>

typedef element_t label_t;

label_t *label_reate(int x, int y, int w, int h, const char *text);
void label_destroy(label_t *btn);
