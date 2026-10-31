#pragma once

#include <server/compositor/compositor.h>
#include <core/object/object.h>

typedef struct {
  object_t *surface; // cursor object; its top left corner is the hot spot
} cursor_t;

/** The arrow pointer. */
cursor_t *cursor_create(void);
void cursor_destroy(cursor_t *c);
void cursor_move(cursor_t *c, int x, int y);
