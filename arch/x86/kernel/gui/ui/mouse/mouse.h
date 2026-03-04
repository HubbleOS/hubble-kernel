#pragma once

#include "../../object.h"
#include "../element.h"
#include "../../compositor.h"

typedef struct
{
	int x, y;		  // screen cursor coordinates
	bool left;		  // left button
	object_t *hover;	  // the object under the cursor
	element_t *hover_el;	  // element under cursor
	element_t *prev_hover_el; // previous element

	object_t *drag_obj; // drag object
	element_t *drag_el; // drag element
	int drag_offset_x;  // cursor offset inside an object/element
	int drag_offset_y;
} local_mouse_t;

extern local_mouse_t g_mouse;

void mouse_update(int new_x, int new_y, bool left_pressed);
