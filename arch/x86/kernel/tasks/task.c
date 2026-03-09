#include <stdint.h>
#include "lab/tasks.h"

#include <gui/core/screen/screen.h>
#include <gui/core/object/object.h>
#include <gui/core/compositor/compositor.h>

#include <gui/ui/button/button.h>
#include <gui/ui/сursor/cursor.h>
#include <gui/ui/mouse/mouse.h>
#include <gui/ui/window/window.h>
#include <gui/ui/text/text.h>
#include <gui/ui/canvas/canvas.h>
#include <gui/ui/background/background.h>

#include <gui/utils/color/color.h>

#ifdef GUI_DEMO
#include <SDL2/SDL.h>
#include <pthread.h>
#include "platform/mouse.h"
static inline void platform_delay(uint32_t ms) { SDL_Delay(ms); }

static pthread_mutex_t gui_lock = PTHREAD_MUTEX_INITIALIZER;
#define LOCK() pthread_mutex_lock(&gui_lock)
#define UNLOCK() pthread_mutex_unlock(&gui_lock)
#else
#include <smp/scheduler.h>
#include <smp/spinlock.h>
#include <smp/smp.h>
#include <hpet/hpet.h>
#include <dev/mouse.h>
static inline void platform_delay(uint32_t ms) { hpet_delay_ms(ms); }

static spinlock_t gui_lock;
#define LOCK() spinlock_acquire(&gui_lock)
#define UNLOCK() spinlock_release(&gui_lock)
#endif

void tasks_init(void)
{
	compositor_init();
	background_create(rgb(20, 20, 20));
	graph_app_init();
	geometry_app_init();
}

void tasks_render(void)
{
	graph_app_render();
	geometry_app_render();
	compositor_render();
}

static void fps_delay(uint32_t fps)
{
	if (fps == 0)
		return;
	platform_delay(1000 / fps);
}

// void maximize_button_click(object_t *obj, int screen_w, int screen_h)
// {
// 	if (obj->maximized)
// 		object_restore(obj);
// 	else
// 		object_maximize(obj, screen_w, screen_h);
// }

#ifdef GUI_DEMO
static void *update_thread(void *arg)
#else
void update_task(void)
#endif
{
	mouse_t *mouse = get_mouse_info();

	while (1)
	{
		LOCK();
		mouse_update(mouse->x, mouse->y, mouse->left);

		if (g_mouse.drag_obj)
		{
			int drag_x = g_mouse.x - g_mouse.drag_offset_x;
			int drag_y = g_mouse.y - g_mouse.drag_offset_y;
			if (g_mouse.drag_el)
				object_move_element(g_mouse.drag_obj, g_mouse.drag_el,
						    drag_x - g_mouse.drag_obj->x,
						    drag_y - g_mouse.drag_obj->y);
			else
			{
				compositor_move_object(g_mouse.drag_obj, drag_x, drag_y);
				compositor_bring_to_front(g_mouse.drag_obj, LAYER_WINDOWS);
			}
		}
		UNLOCK();
		fps_delay(240);
	}
#ifdef GUI_DEMO
	return NULL;
#endif
}

// ─── render ──────────────────────────────────────────────────────────────────

#ifdef GUI_DEMO
static void *render_thread(void *arg)
#else
void render_task(void)
#endif
{
	tasks_init();

	mouse_t *mouse = get_mouse_info();

	cursor_t *cursor = cursor_create(16, 16, rgb(0, 0, 0), rgb(255, 255, 255));

	while (1)
	{
		LOCK();
		if (cursor->surface->x != mouse->x || cursor->surface->y != mouse->y)
			cursor_move(cursor, mouse->x, mouse->y);
		tasks_render();
		UNLOCK();
		fps_delay(60);
	}
#ifdef GUI_DEMO
	return NULL;
#endif
}

// ─── entry point ─────────────────────────────────────────────────────────────

#ifdef GUI_DEMO
void kmain_thread(void)
{
	pthread_t render, update;
	pthread_create(&render, NULL, render_thread, NULL);
	pthread_create(&update, NULL, update_thread, NULL);
	pthread_detach(render);
	pthread_detach(update);
}
#else
void kmain_thread(void)
{
	spinlock_init(&gui_lock, "gui");
	task_t *render = task_create(render_task, 250);
	task_t *update = task_create(update_task, 200);
	scheduler_add_task(render);
	scheduler_add_task(update);
}
#endif
