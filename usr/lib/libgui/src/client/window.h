/**
 * @file window.h
 * @brief A libgui application: windows shown by the display server
 *
 *   if (gui_connect() < 0) ...       // is /bin/desktop running?
 *   window_t *win = window_create(40, 40, 300, 200);
 *   window_addElement(win, button_create(...));
 *   return gui_run();                // until gui_quit() or the server goes
 *
 * A window draws its elements into a buffer it shares with the server
 * (a memfd, see proto/protocol.h); what changed is reported after each
 * batch of events. Input arrives from the server already aimed at the
 * window under the pointer, or the focused one for keys.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <core/object/object.h>

typedef struct window {
  object_t *surface; /* its elements, drawn into the shared buffer */
  uint32_t id;       /* the server knows it by this */
  size_t size;       /* bytes mapped */

  /* What changed since the server was last told, window coordinates. */
  bool damaged;
  int dmg_x, dmg_y, dmg_w, dmg_h;

  struct window *next;
} window_t;

/** Connect to the display server. Returns 0, or -1 with errno set. */
int gui_connect(void);

/** The screen's size, known once connected. */
int gui_screen_width(void);
int gui_screen_height(void);

/** The connection's descriptor, for an application's own poll(). */
int gui_fd(void);

/** Handle what the server sent (call when gui_fd() is readable), then
 * report changes. Returns 0, or -1 once the server has gone away. */
int gui_dispatch(void);

/** Report what changed in every window since last time. */
void gui_flush(void);

/** Handle events until gui_quit() (returns 0) or the server goes away
 * (returns -1). */
int gui_run(void);

/** Make gui_run() return once the current event is handled. */
void gui_quit(void);

/** A new window at @p x, @p y on screen; NULL if it can't be created. */
window_t *window_create(int x, int y, int w, int h);
void window_destroy(window_t *win);

int window_addElement(window_t *win, element_t *el);
