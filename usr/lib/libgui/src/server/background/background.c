#include "background.h"

#include <server/compositor/compositor.h>
#include <core/object/object.h>
#include <server/screen/screen.h>
#include <stdlib.h>
#include <string.h>

static int mix(int a, int b, int t, int range) {
  return a + (b - a) * t / range;
}

object_t *background_create(color_t top, color_t bottom) {
  object_t *bg = object_create(0, 0, fb_width, fb_height, top);
  if (!bg)
    return NULL;

  background_set_gradient(bg, top, bottom);
  compositor_add(bg, LAYER_BG);
  return bg;
}

void background_set_gradient(object_t *bg, color_t top, color_t bottom) {
  if (!bg)
    return;

  int h = bg->height > 1 ? bg->height - 1 : 1;
  for (int y = 0; y < bg->height; y++) {
    color_t c = rgb(mix(get_red(top), get_red(bottom), y, h),
                    mix(get_green(top), get_green(bottom), y, h),
                    mix(get_blue(top), get_blue(bottom), y, h));
    for (int x = 0; x < bg->width; x++)
      bg->buffer[y * bg->width + x] = c;
  }

  compositor_add_damage(LAYER_BG, 0, 0, fb_width, fb_height);
}
