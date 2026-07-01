/**
 * @file pcspkr.c
 * @brief PC speaker driver — PIT channel 2 tone generation
 */
#include <hpet/hpet.h>
#include <hubble/printk.h>
#include <sound/core/dev.h>
#include <stdint.h>

/* -- PIT registers ---------------------------------------- */

#define PIT_CHANNEL2 0x42
#define PIT_CMD 0x43
#define PIT_BASE_FREQ 1193180u
#define SPEAKER_PORT 0x61

/* -- Inline port I/O helpers ------------------------------ */

static inline void outb(uint16_t port, uint8_t val) {
  __asm__ volatile("outb %0, %1" ::"a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
  uint8_t val;
  __asm__ volatile("inb %1, %0" : "=a"(val) : "Nd"(port));
  return val;
}

/* -- Internal helpers ------------------------------------- */

static void busy_wait_ms(uint32_t ms) { hpet_delay_ms(ms); }

static void pcspk_on(uint32_t freq) {
  uint32_t div = PIT_BASE_FREQ / freq;

  outb(PIT_CMD, 0xB6);
  outb(PIT_CHANNEL2, (uint8_t)(div & 0xFF));
  outb(PIT_CHANNEL2, (uint8_t)(div >> 8));

  outb(SPEAKER_PORT, inb(SPEAKER_PORT) | 0x03);
}

static void pcspk_off(void) { outb(SPEAKER_PORT, inb(SPEAKER_PORT) & ~0x03); }

/* -- Driver ops ------------------------------------------- */

static int pcspk_init(void) {
  printk(KERN_OK "[pcspk] init ok\n");
  return 0;
}

static void pcspk_play(uint32_t freq, uint32_t duration_ms) {
  if (freq == 0)
    return;

  pcspk_on(freq);
  busy_wait_ms(duration_ms);
  pcspk_off();
}

static void pcspk_stop(void) { pcspk_off(); }

const struct sound_driver pcspk_driver = {
    .name = "pcspk",
    .init = pcspk_init,
    .play = pcspk_play,
    .stop = pcspk_stop,
};
