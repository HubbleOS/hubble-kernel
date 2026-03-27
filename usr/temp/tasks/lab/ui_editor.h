#pragma once

#include <stdbool.h>

#include <core/element/element.h>
#include <core/object/object.h>
#include <ui/window/window.h>
#include <ui/canvas/canvas.h>

#include <dev/mouse/mouse.h>

typedef struct
{
	window_t *win;	/* вікно що редагується         */
	bool active;	/* editor overlay on/off         */
	bool prev_left; /* стан кнопки на попередньому тику */

	element_t *selected; /* поточно вибраний елемент      */
} ui_editor_t;

/*  API  */

void ui_editor_init(ui_editor_t *ed, window_t *win);
void ui_editor_toggle(ui_editor_t *ed);

/* викликати з main loop замість стандартного drag-блоку */
void ui_editor_update(ui_editor_t *ed, local_mouse_t *mouse);

/* малювати overlay поверх canvas */
void ui_editor_render(ui_editor_t *ed, canvas_t *cnv);

/* серіалізація */
void ui_editor_save(ui_editor_t *ed, const char *path);
void ui_editor_load(ui_editor_t *ed, const char *path);
