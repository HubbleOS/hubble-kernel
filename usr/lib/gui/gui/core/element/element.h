#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#include <gui/utils/color/color.h>

#ifdef GUI_DEMO
#include "platform/keyboard.h"
#else
// #include <dev/keyboard.h>
#endif

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
	// position and size:
	int x, y;
	int width, height;

	// for draw:
	struct object *owner;
	dirty_rect_t dirty_rect;
	uint32_t *buffer;
	element_draw_fn draw;
	bool needs_redraw;

	color_t bg_color;
	color_t text_color;

	bool is_active;
	bool on_focus;

	// styles
	element_style_t active_style;
	element_style_set_t *style_set;

	// data:
	char *text;

	// state and type:
	element_type_t type;
	element_state_t state;

	// mouse event:
	void (*on_mouse_enter)(struct element *);
	void (*on_mouse_leave)(struct element *);
	void (*on_mouse_down)(struct element *);
	void (*on_mouse_up)(struct element *);

	void (*on_click)(void);

	// keyboard event:
	void (*on_key_down)(struct element *, char);
	void (*on_key_up)(struct element *);

	void (*on_key_char)(element_t *, char);
	// void (*on_key_special)(element_t *, key_action_t);
	void (*on_key_special)(element_t *, int);

} element_t;

element_t *element_create(int x, int y, int w, int h);
void element_destroy(element_t *el);

void element_draw(element_t *el);
void element_redraw(element_t *el);
