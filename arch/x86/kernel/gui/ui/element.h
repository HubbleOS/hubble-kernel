#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "../utils/color.h"

typedef enum
{
	UI_BUTTON,
	UI_LABEL,
	UI_TEXTBOX,
	UI_RECT,
} element_type_t;

// typedef struct
// {
// 	int x, y, width, height;
// 	element_type_t type;

// 	uint32_t *buffer;

// 	char *text;
// } element_t;

typedef struct element element_t;

typedef void (*element_draw_fn)(element_t *el);
typedef void (*element_event_fn)(element_t *el, int event);

typedef struct element
{
	int x, y, width, height;
	element_type_t type;

	uint32_t *buffer;
	char *text;

	element_draw_fn draw;
	element_event_fn event;

} element_t;
