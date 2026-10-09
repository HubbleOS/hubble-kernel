/**
 * @file mouse.c
 * @brief PS/2 mouse driver — aux port setup and packet decoding into input
 *        core events
 */
#include "ps2.h"
#include <hpet/hpet.h>
#include <hubble/input.h>
#include <hubble/module.h>
#include <hubble/printk.h>
#include <interrupt/interrupt.h>
#include <io.h>
#include <stdbool.h>
#include <stdint.h>

#define PS2_STATUS_OUTPUT_FULL 0x01
#define PS2_STATUS_INPUT_FULL 0x02
#define PS2_STATUS_AUX_DATA 0x20

#define MOUSE_ACK 0xFA
#define MOUSE_CMD_RESET 0xFF
#define MOUSE_CMD_DEFAULTS 0xF6
#define MOUSE_CMD_ENABLE 0xF4

/* First packet byte */
#define PKT_BUTTONS 0x07
#define PKT_ALWAYS_ONE 0x08
#define PKT_X_SIGN 0x10
#define PKT_Y_SIGN 0x20
#define PKT_OVERFLOW 0xC0

static input_dev_t mouse_input_dev = {
    .name = "ps2-mouse",
};

/* -- IRQ handler ------------------------------------------ */

static const uint16_t button_codes[3] = {BTN_LEFT, BTN_RIGHT, BTN_MIDDLE};

static void mouse_irq(registers_t *regs) {
  (void)regs;
  static uint8_t packet[3];
  static int cycle;
  static uint8_t buttons;

  if ((inb(PS2_STATUS) & (PS2_STATUS_OUTPUT_FULL | PS2_STATUS_AUX_DATA)) !=
      (PS2_STATUS_OUTPUT_FULL | PS2_STATUS_AUX_DATA))
    return;

  uint8_t byte = inb(PS2_DATA);

  /* Resynchronise: a packet always starts with bit 3 set. */
  if (cycle == 0 && !(byte & PKT_ALWAYS_ONE))
    return;

  packet[cycle++] = byte;
  if (cycle < 3)
    return;
  cycle = 0;

  uint8_t flags = packet[0];
  int dx = 0, dy = 0;
  if (!(flags & PKT_OVERFLOW)) {
    dx = packet[1] - ((flags & PKT_X_SIGN) ? 256 : 0);
    dy = packet[2] - ((flags & PKT_Y_SIGN) ? 256 : 0);
  }
  uint8_t changed = (flags & PKT_BUTTONS) ^ buttons;
  buttons = flags & PKT_BUTTONS;

  if (!dx && !dy && !changed)
    return; /* nothing to report: no empty batch */

  if (dx)
    input_event(&mouse_input_dev, EV_REL, REL_X, dx);
  if (dy) /* PS/2 counts up; REL_Y grows downwards */
    input_event(&mouse_input_dev, EV_REL, REL_Y, -dy);
  for (int i = 0; i < 3; i++)
    if (changed & (1 << i))
      input_event(&mouse_input_dev, EV_KEY, button_codes[i], (flags >> i) & 1);

  input_sync(&mouse_input_dev);
}

/* -- Hardware helpers ------------------------------------- */

/* Bounded waits: a machine without a PS/2 aux port must not hang here. */
static bool ps2_wait_status(uint8_t mask, uint8_t want, uint64_t timeout_ms) {
  uint64_t deadline = hpet_get_time_ns() + timeout_ms * 1000000ULL;
  while ((inb(PS2_STATUS) & mask) != want)
    if (hpet_get_time_ns() > deadline)
      return false;
  return true;
}

static bool ps2_send(uint8_t port, uint8_t byte) {
  if (!ps2_wait_status(PS2_STATUS_INPUT_FULL, 0, 50))
    return false;
  outb(port, byte);
  return true;
}

static int ps2_recv(uint64_t timeout_ms) {
  if (!ps2_wait_status(PS2_STATUS_OUTPUT_FULL, PS2_STATUS_OUTPUT_FULL,
                       timeout_ms))
    return -1;
  return inb(PS2_DATA);
}

/** Send a command to the mouse and wait for its ACK. */
static bool mouse_command(uint8_t cmd) {
  return ps2_send(PS2_COMMAND, 0xD4) && ps2_send(PS2_DATA, cmd) &&
         ps2_recv(50) == MOUSE_ACK;
}

static bool mouse_init_hw(void) {
  /* Enable the aux port and its interrupt. */
  if (!ps2_send(PS2_COMMAND, 0xA8) || !ps2_send(PS2_COMMAND, 0x20))
    return false;
  int config = ps2_recv(50);
  if (config < 0)
    return false;
  config |= 0x03;  /* IRQ1 and IRQ12 */
  config &= ~0x20; /* aux clock on */
  config |= 0x40;  /* keyboard relies on set 2 -> set 1 translation */
  if (!ps2_send(PS2_COMMAND, 0x60) || !ps2_send(PS2_DATA, (uint8_t)config))
    return false;

  /* Reset: ACK, then self-test result (0xAA) and device ID. */
  if (!mouse_command(MOUSE_CMD_RESET))
    return false;
  int bat = ps2_recv(750);
  int id = ps2_recv(50);
  printk(KERN_INFO "[mouse] self-test 0x%x, id 0x%x\n", bat, id);

  return mouse_command(MOUSE_CMD_DEFAULTS) && mouse_command(MOUSE_CMD_ENABLE);
}

/* -- Initcall --------------------------------------------- */

static int mouse_initcall(void) {
  ps2_init();

  /* The keyboard IRQ must not swallow the mouse's replies. */
  __asm__ volatile("cli");
  bool ok = mouse_init_hw();
  __asm__ volatile("sti");

  if (!ok) {
    printk(KERN_INFO "[mouse] no PS/2 mouse\n");
    return 0;
  }

  input_set_bit(EV_KEY, mouse_input_dev.evbit);
  input_set_bit(EV_REL, mouse_input_dev.evbit);
  input_register_device(&mouse_input_dev);
  irq_install_handler(12, mouse_irq);

  printk(KERN_OK "[mouse] PS/2 mouse ready\n");
  return 0;
}

module_init(mouse_initcall);
MODULE_NAME("serio_mouse");
