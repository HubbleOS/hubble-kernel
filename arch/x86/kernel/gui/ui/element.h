#pragma once

#include <stdint.h>
#include "../utils/color.h"

typedef enum
{
	UI_BUTTON,
	UI_LABEL,
	UI_TEXTBOX,
} element_type_t;

typedef struct
{
	int x, y, width, height;
	element_type_t type;

	uint32_t *buffer;

	char *text;

} element_t;
