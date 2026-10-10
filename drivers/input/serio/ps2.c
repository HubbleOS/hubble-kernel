/**
 * @file ps2.c
 * @brief PS/2 controller initialisation and port I/O helpers
 */
#include "ps2.h"
#include <hpet/hpet.h>
#include <hubble/printk.h>
#include <io.h>

/* -- Port I/O --------------------------------------------- */

bool ps2_wait_status(uint8_t mask, uint8_t want, uint64_t timeout_ms) {
  uint64_t deadline = hpet_get_time_ns() + timeout_ms * 1000000ULL;
  while ((inb(PS2_STATUS) & mask) != want)
    if (hpet_get_time_ns() > deadline)
      return false;
  return true;
}

bool ps2_send(uint8_t port, uint8_t byte) {
  if (!ps2_wait_status(PS2_STATUS_INPUT_FULL, 0, 50))
    return false;
  outb(port, byte);
  return true;
}

int ps2_recv(uint64_t timeout_ms) {
  if (!ps2_wait_status(PS2_STATUS_OUTPUT_FULL, PS2_STATUS_OUTPUT_FULL,
                       timeout_ms))
    return -1;
  return inb(PS2_DATA);
}

static bool init_done = false;
static bool present = false;

static bool ps2_init_hw(void) {
  /* Nothing decodes port 0x64 on machines without an i8042: the bus
   * floats and every status bit reads as set. */
  if (inb(PS2_STATUS) == 0xFF)
    return false;

  /* 1. Disable devices */
  if (!ps2_send(PS2_COMMAND, 0xAD) || !ps2_send(PS2_COMMAND, 0xA7))
    return false;

  /* 2. Flush output buffer (the buffer is tiny; a stuck bit is no
   * controller) */
  for (int i = 0; inb(PS2_STATUS) & PS2_STATUS_OUTPUT_FULL; i++) {
    if (i == 16)
      return false;
    inb(PS2_DATA);
  }

  /* 3. Read config byte */
  if (!ps2_send(PS2_COMMAND, 0x20))
    return false;
  int config = ps2_recv(50);
  if (config < 0)
    return false;

  config &= ~0x03;
  config |= 0x40;
  if (!ps2_send(PS2_COMMAND, 0x60) || !ps2_send(PS2_DATA, (uint8_t)config))
    return false;

  if (!ps2_send(PS2_COMMAND, 0xAE))
    return false;

  if (!ps2_send(PS2_DATA, 0xF4))
    return false;
  (void)ps2_recv(50);

  if (!ps2_send(PS2_COMMAND, 0x20))
    return false;
  config = ps2_recv(50);
  if (config < 0)
    return false;

  config |= 0x01; /* enable IRQ1 */
  config |= 0x02; /* enable IRQ12 (mouse) */
  config |= 0x40; /* keep translation enabled */

  return ps2_send(PS2_COMMAND, 0x60) && ps2_send(PS2_DATA, (uint8_t)config);
}

bool ps2_init(void) {
  if (init_done)
    return present;

  __asm__ volatile("cli");
  present = ps2_init_hw();
  init_done = true;
  __asm__ volatile("sti");

  if (!present)
    printk(KERN_INFO "[ps2] no i8042 controller\n");
  return present;
}
