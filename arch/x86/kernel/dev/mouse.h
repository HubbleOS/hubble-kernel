#pragma once

#include <stdbool.h>
#include <stdint.h>

void mouse_init();

typedef struct
{
	int32_t x, y;
	bool left, right, middle; // current held state
	bool left_clicked;	  // set on press, you clear it after handling
	bool right_clicked;
} mouse_t;

mouse_t *get_mouse_info(void);
