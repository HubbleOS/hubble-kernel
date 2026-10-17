#include "keyboard.h"

#include <core/element/element.h>

local_keyboard_t g_keyboard = {0};

void keyboard_dispatch(const input_event_t *ev) {
  element_t *el = g_keyboard.focused_el;
  if (!el)
    return;

  if (ev->type == KEY_TYPE_CHAR && el->on_key_char)
    el->on_key_char(el, ev->character);

  if (ev->type == KEY_TYPE_SPECIAL && el->on_key_special)
    el->on_key_special(el, ev->action);
}
