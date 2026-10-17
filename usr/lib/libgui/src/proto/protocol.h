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
  GUI_CREATE = 1, /* window at x, y of w * h; the buffer's fd comes along */
  GUI_DAMAGE,     /* x, y, w, h of the window changed */
  GUI_MOVE,       /* the button went down on the window's background:
                   * move the window with the pointer until it is up */
  GUI_DESTROY,    /* close the window */

  /* Server to client. */
  GUI_SCREEN = 64,  /* first message: the screen is w * h (window 0) */
  GUI_POINTER,      /* pointer at x, y in the window; buttons */
  GUI_LEAVE,        /* the pointer left the window */
  GUI_KEY,          /* key for the focused window */
};

#define GUI_BUTTON_LEFT 0x1

struct gui_msg {
  uint32_t type;   /* enum gui_msg_type */
  uint32_t window; /* the client's id for the window */
  int32_t x, y, w, h;
  uint32_t buttons;  /* GUI_POINTER: GUI_BUTTON_* */
  input_event_t key; /* GUI_KEY */
};
