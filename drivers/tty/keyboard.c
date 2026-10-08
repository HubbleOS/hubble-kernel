/**
 * @file keyboard.c
 * @brief TTY keyboard handler — keymap, modifier tracking, input core handler
 */
#include "keyboard.h"
#include "keymap.h"
#include <drivers/tty/keyboard.h>
#include <drivers/tty/keymap.h>
#include <drivers/tty/tty.h>
#include <hubble/ctype.h>
#include <hubble/input.h>
#include <hubble/module.h>
#include <lib/misc.k.h>
#include <mm/kmalloc.h>
#include <smp/scheduler.h>
#include <smp/spinlock.h>
#include <smp/waitqueue.h>
#include <stdbool.h>
#include <stddef.h>

/* -- Keymap table ----------------------------------------- */

const keymap_entry_t keymap[] = {
    {.id = {KEY_A, false}, 'a', 'A'},
    {.id = {KEY_B, false}, 'b', 'B'},
    {.id = {KEY_C, false}, 'c', 'C'},
    {.id = {KEY_D, false}, 'd', 'D'},
    {.id = {KEY_E, false}, 'e', 'E'},
    {.id = {KEY_F, false}, 'f', 'F'},
    {.id = {KEY_G, false}, 'g', 'G'},
    {.id = {KEY_H, false}, 'h', 'H'},
    {.id = {KEY_I, false}, 'i', 'I'},
    {.id = {KEY_J, false}, 'j', 'J'},
    {.id = {KEY_K, false}, 'k', 'K'},
    {.id = {KEY_L, false}, 'l', 'L'},
    {.id = {KEY_M, false}, 'm', 'M'},
    {.id = {KEY_N, false}, 'n', 'N'},
    {.id = {KEY_O, false}, 'o', 'O'},
    {.id = {KEY_P, false}, 'p', 'P'},
    {.id = {KEY_Q, false}, 'q', 'Q'},
    {.id = {KEY_R, false}, 'r', 'R'},
    {.id = {KEY_S, false}, 's', 'S'},
    {.id = {KEY_T, false}, 't', 'T'},
    {.id = {KEY_U, false}, 'u', 'U'},
    {.id = {KEY_V, false}, 'v', 'V'},
    {.id = {KEY_W, false}, 'w', 'W'},
    {.id = {KEY_X, false}, 'x', 'X'},
    {.id = {KEY_Y, false}, 'y', 'Y'},
    {.id = {KEY_Z, false}, 'z', 'Z'},

    {.id = {KEY_1, false}, '1', '!'},
    {.id = {KEY_2, false}, '2', '@'},
    {.id = {KEY_3, false}, '3', '#'},
    {.id = {KEY_4, false}, '4', '$'},
    {.id = {KEY_5, false}, '5', '%'},
    {.id = {KEY_6, false}, '6', '^'},
    {.id = {KEY_7, false}, '7', '&'},
    {.id = {KEY_8, false}, '8', '*'},
    {.id = {KEY_9, false}, '9', '('},
    {.id = {KEY_0, false}, '0', ')'},

    {.id = {KEY_SPACE, false}, ' ', ' '},
    /* What a terminal sends: Enter is CR (the tty's ICRNL makes it NL
     * for cooked readers), Backspace is DEL (termios VERASE). */
    {.id = {KEY_ENTER, false}, '\r', '\r'},
    {.id = {KEY_TAB, false}, '\t', '\t'},
    {.id = {KEY_ESC, false}, 27, 27},
    {.id = {KEY_BACKSPACE, false}, 0x7F, 0x7F},

    {.id = {KEY_MINUS, false}, '-', '_'},
    {.id = {KEY_EQUAL, false}, '=', '+'},
    {.id = {KEY_LEFT_BRACKET, false}, '[', '{'},
    {.id = {KEY_RIGHT_BRACKET, false}, ']', '}'},
    {.id = {KEY_BACKSLASH, false}, '\\', '|'},
    {.id = {KEY_SEMICOLON, false}, ';', ':'},
    {.id = {KEY_APOSTROPHE, false}, '\'', '\"'},
    {.id = {KEY_COMMA, false}, ',', '<'},
    {.id = {KEY_PERIOD, false}, '.', '>'},
    {.id = {KEY_SLASH, false}, '/', '?'},
    {.id = {KEY_CAPS_LOCK, false}, 0, 0},
    {.id = {KEY_GRAVE, false}, '`', '~'},

    {.id = {KEY_LEFT_SHIFT, false}, 0, 0},
    {.id = {KEY_RIGHT_SHIFT, false}, 0, 0},
    {.id = {KEY_LEFT_CTRL, false}, 0, 0},
    {.id = {KEY_RIGHT_CTRL, true}, 0, 0},
    {.id = {KEY_LEFT_ALT, false}, 0, 0},
    {.id = {KEY_RIGHT_ALT, true}, 0, 0},
};

const size_t keymap_size = SIZEOF_ARRAY(keymap);

/* -- Keymap lookup ---------------------------------------- */

char keymap_lookup_char(uint8_t scancode, bool extended, bool shift,
                        bool caps) {
  for (size_t i = 0; i < keymap_size; ++i)
    if (keymap[i].id.scancode == scancode &&
        keymap[i].id.extended == extended) {
      char c = shift ? keymap[i].shifted : keymap[i].normal;

      if (isalpha(c) && caps)
        c = shift ? tolower(c) : toupper(c);

      return c;
    }
  return 0;
}

/* -- Modifier state tracking ------------------------------ */

typedef struct {
  bool shift;
  bool ctrl;
  bool alt;
  bool caps_lock;
} kbd_state_t;

static kbd_state_t kbd_state = {0};

static void update_modifiers(uint16_t code, bool pressed) {
  uint8_t sc = code & 0xFF;
  bool extended = code & 0x100;

  if (sc == KEY_LEFT_SHIFT || sc == KEY_RIGHT_SHIFT)
    kbd_state.shift = pressed;
  else if ((sc == KEY_LEFT_CTRL && !extended) ||
           (sc == KEY_RIGHT_CTRL && extended))
    kbd_state.ctrl = pressed;
  else if ((sc == KEY_LEFT_ALT && !extended) ||
           (sc == KEY_RIGHT_ALT && extended))
    kbd_state.alt = pressed;
  else if (sc == KEY_CAPS_LOCK && pressed)
    kbd_state.caps_lock = !kbd_state.caps_lock;
}

/* -- Key event handler ------------------------------------ */

static void tty_handle_key(uint16_t code, bool pressed) {
  uint8_t sc = code & 0xFF;
  bool extended = code & 0x100;

  if (!pressed)
    return;

  /* Ctrl+letter (and Ctrl+[ \\ ] ^ _) sends the control code, as on any
   * terminal: ^C = 0x03, ^D = VEOF, ^L = form feed, ^[ = ESC. */
  if (kbd_state.ctrl && !extended) {
    char base = keymap_lookup_char(sc, false, false, false);
    if ((base >= 'a' && base <= 'z') || (base >= '[' && base <= '_')) {
      tty_input_char(tty_current, (char)(base & 0x1F));
      return;
    }
  }

  /* Navigation keys arrive as the VT100/xterm input sequences, fed to
   * the reader like typed bytes (they used to be written to the screen). */
  if (extended) {
    const char *seq = NULL;
    switch (sc) {
    case KEY_UP:
      seq = "\x1b[A";
      break;
    case KEY_DOWN:
      seq = "\x1b[B";
      break;
    case KEY_RIGHT:
      seq = "\x1b[C";
      break;
    case KEY_LEFT:
      seq = "\x1b[D";
      break;
    case KEY_HOME:
      seq = "\x1b[H";
      break;
    case KEY_END:
      seq = "\x1b[F";
      break;
    case KEY_DELETE:
      seq = "\x1b[3~";
      break;
    case KEY_INSERT:
      seq = "\x1b[2~";
      break;
    case KEY_PAGEUP:
      seq = "\x1b[5~";
      break;
    case KEY_PAGEDOWN:
      seq = "\x1b[6~";
      break;
    }
    if (seq)
      tty_input_string(tty_current, seq);
    return;
  }

  if (sc >= KEY_F1 && sc <= KEY_F10)
    return;

  char c =
      keymap_lookup_char(sc, extended, kbd_state.shift, kbd_state.caps_lock);
  if (c != 0)
    tty_input_char(tty_current, c);
}

/* -- Input handler ---------------------------------------- */

static bool tty_kbd_match(input_handler_t *handler, input_dev_t *dev) {
  (void)handler;
  return input_test_bit(EV_KEY, dev->evbit);
}

/* Every keyboard (PS/2, USB, ...) needs its own handle: a handle is a
 * node in that device's handle list. */
static int tty_kbd_connect(input_handler_t *handler, input_dev_t *dev) {
  input_handle_t *handle = kzalloc(sizeof(*handle));
  if (!handle)
    return -1;

  handle->dev = dev;
  handle->handler = handler;

  input_link_handle(handle);
  return 0;
}

static void tty_kbd_disconnect(input_handle_t *handle) {
  input_unlink_handle(handle);
  kfree(handle);
}

/* Keyboards report from IRQ context (PS/2) and from kernel threads (USB),
 * possibly on different CPUs at once; the modifier state and the tty
 * line buffer must see one key at a time. */
static irqlock_t tty_kbd_lock = IRQLOCK_INIT("tty_kbd");

static void tty_kbd_event(input_handle_t *handle, input_raw_event_t *ev) {
  (void)handle;

  if (ev->type != EV_KEY)
    return;
  if (!tty_current)
    return;

  bool pressed = ev->value == 1;
  bool repeat = ev->value == 2;

  irqlock_acquire(&tty_kbd_lock);

  update_modifiers(ev->code, pressed);

  if (pressed || repeat)
    tty_handle_key(ev->code, true);
  else
    tty_handle_key(ev->code, false);

  irqlock_release(&tty_kbd_lock);
}

static input_handler_t tty_kbd_handler = {
    .name = "tty-keyboard",
    .match = tty_kbd_match,
    .connect = tty_kbd_connect,
    .disconnect = tty_kbd_disconnect,
    .event = tty_kbd_event,
};

/* -- Initcall --------------------------------------------- */

static int tty_keyboard_initcall(void) {
  input_register_handler(&tty_kbd_handler);
  return 0;
}

module_init(tty_keyboard_initcall);
MODULE_NAME("tty_keyboard");
