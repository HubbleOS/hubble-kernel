#pragma once

#include "../element.h"

typedef struct
{
	element_t base;
} text_t;

text_t *element_create_text(int x, int y, const char *text);
