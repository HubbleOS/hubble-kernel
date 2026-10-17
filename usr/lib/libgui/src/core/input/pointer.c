#include "pointer.h"

#include <stddef.h>

/* Topmost element of @p obj at (x, y): the last one added wins. */
static element_t *element_at(object_t *obj, int x, int y) {
  for (int e = obj->element_count - 1; e >= 0; e--) {
    element_t *el = obj->elements[e];
    if (x >= el->x && x < el->x + el->width && y >= el->y &&
        y < el->y + el->height)
      return el;
  }
  return NULL;
}

static void set_hover(pointer_t *p, element_t *el) {
  if (el == p->hover_el)
    return;
  if (p->hover_el && p->hover_el->on_mouse_leave)
    p->hover_el->on_mouse_leave(p->hover_el);
  if (el && el->on_mouse_enter)
    el->on_mouse_enter(el);
  p->hover_el = el;
}

void pointer_leave(pointer_t *p) {
  set_hover(p, NULL);
  p->pressed_el = NULL;
  p->drag_el = NULL;
  p->obj = NULL;
  p->left = false;
}

bool pointer_update(pointer_t *p, object_t *obj, int x, int y, bool left) {
  if (obj != p->obj) {
    pointer_leave(p);
    p->obj = obj;
  }

  element_t *hover = element_at(obj, x, y);
  bool move_window = false;

  if (left && !p->left) {
    if (hover && hover->on_mouse_down) {
      hover->on_mouse_down(hover);
      p->pressed_el = hover;
    }
    /* Buttons are clicked; anything else is picked up and dragged. */
    if (hover && hover->type != UI_BUTTON) {
      p->drag_el = hover;
      p->drag_offset_x = x - hover->x;
      p->drag_offset_y = y - hover->y;
    } else if (!hover) {
      move_window = true;
    }
  }

  if (!left && p->left) {
    if (p->pressed_el && p->pressed_el->on_mouse_up)
      p->pressed_el->on_mouse_up(p->pressed_el);
    p->pressed_el = NULL;
    p->drag_el = NULL;
  }

  set_hover(p, hover);

  if (p->drag_el && left) {
    int nx = x - p->drag_offset_x, ny = y - p->drag_offset_y;
    if (nx != p->drag_el->x || ny != p->drag_el->y)
      object_move_element(obj, p->drag_el, nx, ny);
  }

  p->left = left;
  return move_window;
}
