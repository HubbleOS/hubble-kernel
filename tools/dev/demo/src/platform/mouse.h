#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
	int32_t x, y;
	bool left, right, middle;
	bool left_clicked;
	bool right_clicked;
} mouse_t;

#include <gui/ui/mouse/mouse.h>

extern mouse_t *mouse;

mouse_t *get_mouse_info(void);
