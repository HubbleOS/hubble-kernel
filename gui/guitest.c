
#include "main.h"

#include <gui/core/screen/screen.h>
#include <gui/core/object/object.h>
#include <gui/core/compositor/compositor.h>

#include <gui/ui/button/button.h>
#include <gui/ui/сursor/cursor.h>
#include <gui/ui/mouse/mouse.h>
#include <gui/ui/window/window.h>
#include <gui/ui/rect/rect.h>
#include <gui/ui/text/text.h>
#include <gui/ui/canvas/canvas.h>
#include <lab/tasks.h>

#include "gui/background.h"

extern cursor_t *cursor;
extern mouse_t *m;

void kmain(framebuffer_info_t *fb)
{

	graph_app_update();
	geometry_app_update();

	mouse_update(m->x, m->y, m->left);
	if (cursor->x != m->x || cursor->y != m->y)
	{
		cursor_move(cursor, m->x, m->y);
	}

	if (g_mouse.drag_obj)
	{
		if (g_mouse.drag_el)
			object_move_element(g_mouse.drag_obj, g_mouse.drag_el,
					    g_mouse.x - g_mouse.drag_offset_x - g_mouse.drag_obj->x,
					    g_mouse.y - g_mouse.drag_offset_y - g_mouse.drag_obj->y);
		else
			compositor_move_object(g_mouse.drag_obj,
					       g_mouse.x - g_mouse.drag_offset_x,
					       g_mouse.y - g_mouse.drag_offset_y);

		compositor_bring_to_front(g_mouse.drag_obj, LAYER_WINDOWS);
	}

	compositor_render();
}
