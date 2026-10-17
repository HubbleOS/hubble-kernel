/**
 * @file display.h
 * @brief The display server: libgui's compositor serving client windows
 *
 * Owns the screen, the background and the pointer, listens on GUI_SOCKET
 * (proto/protocol.h) and shows each client window from the buffer the
 * client shares with it. The platform (apps/desktop) feeds it pointer and
 * key input and polls its descriptors:
 *
 *   display_init(&fb);
 *   for (;;) {
 *     display_render();
 *     n = display_poll_fds(fds, max);  // plus the platform's own
 *     poll(...);
 *     display_handle(fds, n);
 *     ... display_pointer() / display_key() for input ...
 *   }
 */
#pragma once

#include <poll.h>
#include <stdbool.h>

#include <core/input/keyboard.h>
#include <server/screen/screen.h>

/** Take over @p fb and start listening. Returns 0, or -1 with errno. */
int display_init(framebuffer_info_t *fb);

/** The descriptors to poll (listener and clients); returns how many. */
int display_poll_fds(struct pollfd *fds, int max);

/** Act on what poll() reported for descriptors from display_poll_fds. */
void display_handle(const struct pollfd *fds, int n);

/** The pointer is at @p x, @p y on screen, left button @p left. */
void display_pointer(int x, int y, bool left);

/** A key, for the focused window. */
void display_key(const input_event_t *ev);

/** Put what changed on screen. */
void display_render(void);
