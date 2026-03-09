#include "mouse.h"
#include "../button/button.h"
#include <stddef.h>

local_mouse_t g_mouse = {0};

static void mouse_update_hover(local_mouse_t *mouse);
static void mouse_handle_hover_events(local_mouse_t *mouse);
static void mouse_handle_button_events(local_mouse_t *mouse, bool left_pressed);
static void mouse_handle_drag(local_mouse_t *mouse, bool left_pressed);

void mouse_update(int new_x, int new_y, bool left_pressed)
{
	local_mouse_t *mouse = &g_mouse;

	mouse->x = new_x;
	mouse->y = new_y;

	mouse->hover = NULL;
	mouse->hover_el = NULL;

	mouse_update_hover(mouse);
	mouse_handle_button_events(mouse, left_pressed);
	mouse_handle_hover_events(mouse);
	mouse_handle_drag(mouse, left_pressed);

	mouse->left = left_pressed;
}

static void mouse_update_hover(local_mouse_t *mouse)
{
	for (int l = LAYER_WINDOWS; l >= 0; l--)
	{
		layer_t *layer = &compositor.layers[l];

		for (int i = layer->count - 1; i >= 0; i--)
		{
			object_t *obj = layer->objects[i];

			for (int e = obj->element_count - 1; e >= 0; e--)
			{
				element_t *el = obj->elements[e];
				int el_x = obj->x + el->x;
				int el_y = obj->y + el->y;

				if (mouse->x >= el_x && mouse->x < el_x + el->width &&
				    mouse->y >= el_y && mouse->y < el_y + el->height)
				{
					mouse->hover = obj;
					mouse->hover_el = el;
					return;
				}
			}

			if (mouse->x >= obj->x && mouse->x < obj->x + obj->width &&
			    mouse->y >= obj->y && mouse->y < obj->y + obj->height)
			{
				mouse->hover = obj;
				mouse->hover_el = NULL;
				return;
			}
		}
	}
}

static void mouse_handle_hover_events(local_mouse_t *mouse)
{
	if (mouse->hover_el != mouse->prev_hover_el)
	{
		if (mouse->prev_hover_el && mouse->prev_hover_el->on_mouse_leave)
			mouse->prev_hover_el->on_mouse_leave(mouse->prev_hover_el);

		if (mouse->hover_el && mouse->hover_el->on_mouse_enter)
			mouse->hover_el->on_mouse_enter(mouse->hover_el);

		mouse->prev_hover_el = mouse->hover_el;
	}
}

void maximize_button_click(object_t *obj, int screen_w, int screen_h)
{
	if (obj->maximized)
		object_restore(obj);
	else
		object_maximize(obj, screen_w, screen_h);
}

#include <gui/core/screen/screen.h>

static void mouse_handle_button_events(local_mouse_t *mouse, bool left_pressed)
{
	if (left_pressed && !mouse->left)
	{
		if (mouse->hover && mouse->hover->layer == LAYER_WINDOWS)
		{

			compositor_bring_to_front(mouse->hover, LAYER_WINDOWS);
			maximize_button_click(mouse->hover, g_fb->width, g_fb->height);
		}

		if (mouse->hover_el && mouse->hover_el->on_mouse_down)
		{
			mouse->hover_el->on_mouse_down(mouse->hover_el);
			mouse->pressed_el = mouse->hover_el;
		}
	}

	if (!left_pressed && mouse->left)
	{
		if (mouse->pressed_el && mouse->pressed_el->on_mouse_up)
			mouse->pressed_el->on_mouse_up(mouse->pressed_el);
		mouse->pressed_el = NULL;
	}
}

static void mouse_handle_drag(local_mouse_t *mouse, bool left_pressed)
{
	if (left_pressed && !mouse->drag_obj)
	{
		if (!mouse->hover)
			return;
		if (mouse->hover->layer == LAYER_CURSOR)
			return;
		if (mouse->hover->layer == LAYER_BG)
			return;

		if (mouse->hover_el && mouse->hover_el->type == UI_BUTTON)
			return;

		if (mouse->hover_el)
		{

			mouse->drag_obj = mouse->hover;
			mouse->drag_el = mouse->hover_el;
			mouse->drag_offset_x = mouse->x - (mouse->drag_obj->x + mouse->drag_el->x);
			mouse->drag_offset_y = mouse->y - (mouse->drag_obj->y + mouse->drag_el->y);
		}
		else
		{

			mouse->drag_obj = mouse->hover;
			mouse->drag_el = NULL;
			mouse->drag_offset_x = mouse->x - mouse->drag_obj->x;
			mouse->drag_offset_y = mouse->y - mouse->drag_obj->y;
		}
	}
	else if (!left_pressed)
	{
		mouse->drag_obj = NULL;
		mouse->drag_el = NULL;
	}
}
