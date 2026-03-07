#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#include <gui/utils/color/color.h>

#include "styles/style.h"

typedef enum
{
	UI_BUTTON,
	UI_LABEL,
	UI_TEXTBOX,
	UI_RECT,
} element_type_t;

typedef enum
{
	UI_EVENT_MOUSE_ENTER,
	UI_EVENT_MOUSE_LEAVE,
	UI_EVENT_MOUSE_DOWN,
	UI_EVENT_MOUSE_UP,
} ui_event_t;

typedef struct element element_t;

typedef void (*element_draw_fn)(element_t *el);
typedef void (*element_event_fn)(element_t *el, int event);

typedef struct
{
	int x, y, w, h;
	bool valid;
} dirty_rect_t;

typedef enum
{
	ELEMENT_NORMAL,
	ELEMENT_HOVER,
	ELEMENT_PRESSED,
} element_state_t;

typedef struct element
{
	int x, y;
	int width, height;

	element_style_t active_style;
	element_style_set_t *style_set;

	color_t bg_color;
	color_t text_color;

	uint32_t *buffer;
	char *text;

	dirty_rect_t dirty_rect;
	struct object *owner;

	element_type_t type;
	element_state_t state;

	void (*on_mouse_enter)(struct element *);
	void (*on_mouse_leave)(struct element *);
	void (*on_mouse_down)(struct element *);
	void (*on_mouse_up)(struct element *);

	void (*on_click)(void);

	void (*draw)(struct element *);
	bool needs_redraw;
} element_t;

void element_init(element_t *el);

element_t *element_create(int x, int y, int w, int h);
void element_destroy(element_t *el);
