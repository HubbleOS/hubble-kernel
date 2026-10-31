/**
 * @file protocol.h
 * @brief The libgui display protocol: what clients and the display server
 *        (/bin/desktop) send each other over GUI_SOCKET
 *
 * Every message is one struct gui_msg. A window's pixels never travel
 * through the socket: the client creates a memfd holding the window's
 * buffer (width * height pixels, 0xAARRGGBB, row after row, alpha 255
 * for opaque), passes it with GUI_CREATE (SCM_RIGHTS), and both sides
 * map the same memory. After drawing, the client sends GUI_DAMAGE with
 * what changed, and the server puts that on screen.
 *
 * Both sides are built from the same libgui, so the structs are shared
 * as they are.
 */
#pragma once

#include <stdint.h>

#include <core/input/keyboard.h>

#define GUI_SOCKET "/tmp/gui.sock"

#define GUI_MAX_SIZE 4096 /* largest window side, pixels */

enum gui_msg_type {
  /* Client to server. Window ids are the client's, one per window. */
  GUI_CREATE = 1, /* window at x, y of w * h, GUI_WINDOW_* flags; the
                   * buffer's fd comes along */
  GUI_DAMAGE,     /* x, y, w, h of the window changed */
  GUI_MOVE,       /* the button went down on the window's background:
                   * move the window with the pointer until it is up */
  GUI_DESTROY,    /* close the window */
  GUI_TITLE,      /* the window's title bar shows title */

  /* Server to client. */
  GUI_SCREEN = 64,  /* first message: the screen is w * h (window 0) */
  GUI_POINTER,      /* pointer at x, y in the window; buttons */
  GUI_LEAVE,        /* the pointer left the window */
  GUI_KEY,          /* key for the focused window */
  GUI_CLOSE,        /* the title bar's close button was clicked */
};

#define GUI_BUTTON_LEFT 0x1

/* GUI_CREATE flags. A panel (a taskbar) has no title bar, stays where it
 * was put and above the other windows. Otherwise the server frames the
 * window: x, y is still where its own pixels go, the title bar above. */
#define GUI_WINDOW_PANEL 0x1

#define GUI_TITLE_MAX 48 /* with the terminating NUL */

struct gui_msg {
  uint32_t type;   /* enum gui_msg_type */
  uint32_t window; /* the client's id for the window */
  int32_t x, y, w, h;
  uint32_t buttons;  /* GUI_POINTER: GUI_BUTTON_* */
  uint32_t flags;    /* GUI_CREATE: GUI_WINDOW_* */
  input_event_t key; /* GUI_KEY */
  char title[GUI_TITLE_MAX]; /* GUI_TITLE; may lack the NUL */
};
