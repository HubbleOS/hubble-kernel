#include <gui/core/compositor/compositor.h>
#include <gui/core/screen/screen.h>
#include <stdlib.h>
#include <string.h>
#include <gui/core/object/object.h>

object_t *background_create(color_t color)
{
	object_t *bg = object_create(0, 0, fb_width, fb_height, color);
	if (!bg)
		return NULL;

	compositor_add(bg, LAYER_BG);
	return bg;
}

void background_set_color(object_t *bg, color_t color)
{
	if (!bg)
		return;

	for (int i = 0; i < bg->width * bg->height; i++)
		bg->buffer[i] = color;

	compositor_add_damage(LAYER_BG, 0, 0, fb_width, fb_height);
}
