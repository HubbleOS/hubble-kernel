#pragma once

#include <gui/core/element/element.h>

typedef element_t text_t;

text_t *element_create_text(int x, int y, const char *text);
void destroy_text(text_t *txt);
