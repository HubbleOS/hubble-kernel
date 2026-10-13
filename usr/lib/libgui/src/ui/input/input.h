#pragma once

#include <core/element/element.h>

typedef element_t input_t;

input_t *input_create(int x, int y, int w, int h, const char *text);
void input_destroy(input_t *input);
