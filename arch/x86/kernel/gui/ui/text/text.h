#pragma once

#include <gui/core/element/element.h>

typedef element_t text_t;

text_t *element_create_text(int x, int y, const char *text);
void text_destroy(text_t *txt);
