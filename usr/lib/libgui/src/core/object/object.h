#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <core/element/element.h>
#include <utils/color/color.h>

typedef struct object {
  int x, y;
  int width, height;

  uint32_t *buffer; /* width * height ARGB pixels */
  bool owns_buffer; /* allocated here, freed with the object */
  color_t bg_color;

  /* Who shows the object: told each rectangle that changed, in object
   * coordinates. The compositor puts it on screen; a client window
   * sends it to the display server. NULL: nobody yet. */
  void (*on_damage)(struct object *obj, int x, int y, int w, int h);
  void *host; /* the host's own data */

  element_t **elements;
  int element_count;
  int element_capacity;

  int layer;
} object_t;

void element_mark_dirty(element_t *el);
void element_mark_dirty_rect(element_t *el, int x, int y, int w, int h);

void object_flush(object_t *obj);

/** An object with its own buffer, filled with @p bg_color. */
object_t *object_create(int x, int y, int w, int h, color_t bg_color);

/** An object drawing into (or showing) @p buffer, which the caller owns
 * and keeps alive; its pixels are left as they are. */
object_t *object_create_on(int x, int y, int w, int h, color_t bg_color,
                           uint32_t *buffer);

void object_destroy(object_t *obj);

/** Tell the host that [x, y, w, h] of @p obj changed. */
void object_damage(object_t *obj, int x, int y, int w, int h);

void object_redraw_elements(object_t *obj);
void object_add_element(object_t *obj, element_t *el);
void object_move_element(object_t *obj, element_t *el, int new_x, int new_y);
