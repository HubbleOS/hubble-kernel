/**
 * @file keyboard.c
 * @brief PS/2 keyboard driver — scancode processing, IRQ, input core
 * registration
 */
#include "keyboard.h"
#include "ps2.h"
#include <asm.h>
#include <drivers/tty/keyboard.h>
#include <drivers/tty/keymap.h>
#include <hubble/input.h>
#include <hubble/module.h>
#include <hubble/printk.h>
#include <hubble/string.h>
#include <io.h>
#include <smp/waitqueue.h>
#include <stddef.h>

/* -- Scancode processing ---------------------------------- */

static bool process_scancode_once(uint8_t raw, key_event_t *out_evt) {
  static bool extended = false;
  static bool shift_pressed = false;
  static bool ctrl_pressed = false;
  static bool alt_pressed = false;
  static bool caps_lock_active = false;

  uint8_t scancode = GET_SCANCODE(raw);
  bool released = IS_RELEASED(raw);

  if (IS_EXTENDED(raw)) {
    extended = true;
    return false;
  }

  if (scancode == KEY_LEFT_SHIFT || scancode == KEY_RIGHT_SHIFT)
    shift_pressed = !released;
  else if ((scancode == KEY_LEFT_CTRL && !extended) ||
           (scancode == KEY_RIGHT_CTRL && extended))
    ctrl_pressed = !released;
  else if ((scancode == KEY_LEFT_ALT && !extended) ||
           (scancode == KEY_RIGHT_ALT && extended))
    alt_pressed = !released;
  else if (scancode == KEY_CAPS_LOCK && !released)
    caps_lock_active = !caps_lock_active;

  out_evt->id.scancode = scancode;
  out_evt->id.extended = extended;
  out_evt->released = released;
  out_evt->is_shift = shift_pressed;
  out_evt->is_ctrl = ctrl_pressed;
  out_evt->is_alt = alt_pressed;
  out_evt->is_caps_lock = caps_lock_active;

  extended = false;
  return true;
}

/* -- Input device ----------------------------------------- */

static input_dev_t kbd_input_dev = {
    .name = "ps2-keyboard",
};

/* -- IRQ handler ------------------------------------------ */

void keyboard_irq(registers_t *r) {
  (void)r;
  uint8_t status = inb(0x64);
  if ((status & 0x20))
    return;
  if (!(status & 0x01))
    return;

  uint8_t raw = inb(0x60);

  key_event_t evt;
  if (!process_scancode_once(raw, &evt))
    return;

  input_raw_event_t ev = {
      .type = EV_KEY,
      .code = evt.id.scancode | (evt.id.extended ? 0x100 : 0),
      .value = evt.released ? 0 : 1,
  };

  input_report(&kbd_input_dev, &ev);
}

/* -- Hardware helpers ------------------------------------- */

static void keyboard_write(uint8_t cmd) {
  ps2_wait_input();
  outb(PS2_DATA, cmd);
}

static uint8_t keyboard_read(void) {
  ps2_wait_output();
  return inb(PS2_DATA);
}

static void keyboard_init_hw(void) {
  __asm__ volatile("cli");

  uint8_t act;
  keyboard_write(0xF0);
  act = keyboard_read();
  printk(KERN_INFO "Keyboard active: %02x\n", act);

  keyboard_write(0x01);
  act = keyboard_read();
  printk(KERN_INFO "Keyboard active: %02x\n", act);

  __asm__ volatile("sti");
  printk(KERN_OK "Keyboard initialized\n");
}

/* -- Initcall --------------------------------------------- */

static int keyboard_initcall(void) {
  ps2_init();
  keyboard_init_hw();

  input_set_bit(EV_KEY, kbd_input_dev.evbit);

  input_register_device(&kbd_input_dev);
  irq_install_handler(1, keyboard_irq);

  return 0;
}

module_init(keyboard_initcall);
MODULE_NAME("serio_keyboard");
