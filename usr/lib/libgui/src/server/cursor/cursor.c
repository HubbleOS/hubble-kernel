#include "cursor.h"

#include <stdlib.h>
#include <string.h>

/* B: outline, W: fill, anything else: transparent. */
static const char *const arrow[] = {
    "B           ",
    "BB          ",
    "BWB         ",
    "BWWB        ",
    "BWWWB       ",
    "BWWWWB      ",
    "BWWWWWB     ",
    "BWWWWWWB    ",
    "BWWWWWWWB   ",
    "BWWWWWWWWB  ",
    "BWWWWWWWWWB ",
    "BWWWWWWBBBBB",
    "BWWWBWWB    ",
    "BWWB BWWB   ",
    "BWB  BWWB   ",
    "BB    BWWB  ",
    "B     BWWB  ",
    "       BWWB ",
    "        BB  ",
};

#define ARROW_W 12
#define ARROW_H ((int)(sizeof(arrow) / sizeof(arrow[0])))

cursor_t *cursor_create(void) {
  cursor_t *c = malloc(sizeof(cursor_t));
  if (!c)
    return NULL;

  c->surface = object_create(0, 0, ARROW_W, ARROW_H, 0);
  if (!c->surface) {
    free(c);
    return NULL;
  }

  for (int y = 0; y < ARROW_H; y++)
    for (int x = 0; x < ARROW_W; x++) {
      char p = arrow[y][x];
      c->surface->buffer[y * ARROW_W + x] =
          p == 'B' ? rgb(0, 0, 0) : p == 'W' ? rgb(255, 255, 255) : 0;
    }

  compositor_add(c->surface, LAYER_CURSOR);
  return c;
}

void cursor_destroy(cursor_t *c) {
  if (!c)
    return;

  compositor_remove(c->surface, LAYER_CURSOR);
  object_destroy(c->surface);
  free(c);
}

void cursor_move(cursor_t *c, int x, int y) {
  if (!c)
    return;
  compositor_move_object(c->surface, x, y);
}
