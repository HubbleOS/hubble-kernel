/**
 * @file pointer.h
 * @brief The pointer inside one window: hover, press, click, drag
 *
 * The display server decides which window the pointer is over and sends
 * its position in that window's coordinates; this turns those positions
 * into element events (enter/leave, down/up, click) and element drags.
 * Pressing the window's background asks for the whole window to move,
 * which only the server can do.
 */
#pragma once

#include <stdbool.h>

#include <core/element/element.h>
#include <core/object/object.h>

typedef struct {
  object_t *obj;        /* window the pointer is in, or NULL */
  bool left;            /* left button */
  element_t *hover_el;  /* element under the pointer */
  element_t *pressed_el;
  element_t *drag_el;   /* element being dragged inside the window */
  int drag_offset_x;    /* pointer offset inside drag_el */
  int drag_offset_y;
} pointer_t;

/**
 * The pointer is at (x, y) in @p obj, left button @p left.
 * Returns true when the button went down on the window's background:
 * the caller should start moving the window.
 */
bool pointer_update(pointer_t *p, object_t *obj, int x, int y, bool left);

/** The pointer left its window. */
void pointer_leave(pointer_t *p);
