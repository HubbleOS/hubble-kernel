/**
 * libgui's display server half: the compositor, the screen and the pointer
 * cursor, serving client windows over GUI_SOCKET. Only /bin/desktop uses
 * it; applications use libgui.h.
 */
#pragma once

#include <libgui/core.h>
#include <libgui/utils.h>
#include <proto/protocol.h>
#include <server/display.h>
