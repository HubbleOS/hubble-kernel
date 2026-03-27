#pragma once

typedef struct element element_t;

#include <utils/color/color.h>

typedef struct
{
	color_t background_color;
	color_t text_color;

	int border_radius;
} element_style_t;

typedef struct
{
	element_style_t *normal;
	element_style_t *hover;
	element_style_t *pressed;
} element_style_set_t;

void element_style_init(element_style_t *style);

void element_apply_style(element_t *el, element_style_t *override);
