#pragma once

#include <gui/core/element/element.h>

typedef element_t label_t;

label_t *label_create(int x, int y, int w, int h, const char *text);
void label_destroy(label_t *label);

void label_set_text(label_t *label, const char *text);
