#include <sound/core/dev.h>
#include <hubble/printk.h>
#include <stdint.h>

// PIT channel 2
#define PIT_CHANNEL2 0x42
#define PIT_CMD 0x43
#define PIT_BASE_FREQ 1193180u

// Port 0x61 — speaker gate
#define SPEAKER_PORT 0x61

static inline void outb(uint16_t port, uint8_t val)
{
	__asm__ volatile("outb %0, %1" ::"a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port)
{
	uint8_t val;
	__asm__ volatile("inb %1, %0" : "=a"(val) : "Nd"(port));
	return val;
}

#include <hpet/hpet.h>

static void busy_wait_ms(uint32_t ms)
{
	hpet_delay_ms(ms);
}

static void pcspk_on(uint32_t freq)
{
	uint32_t div = PIT_BASE_FREQ / freq;

	// PIT channel 2, mode 3 (square wave), binary
	outb(PIT_CMD, 0xB6);
	outb(PIT_CHANNEL2, (uint8_t)(div & 0xFF));
	outb(PIT_CHANNEL2, (uint8_t)(div >> 8));

	// turn on the speaker (bits 0 and 1)
	outb(SPEAKER_PORT, inb(SPEAKER_PORT) | 0x03);
}

static void pcspk_off(void)
{
	outb(SPEAKER_PORT, inb(SPEAKER_PORT) & ~0x03);
}

static int pcspk_init(void)
{
	printk("[pcspk] init ok\n");
	return 0;
}

static void pcspk_play(uint32_t freq, uint32_t duration_ms)
{
	if (freq == 0)
		return;

	pcspk_on(freq);
	busy_wait_ms(duration_ms);
	pcspk_off();
}

static void pcspk_stop(void)
{
	pcspk_off();
}

const struct sound_driver pcspk_driver = {
    .name = "pcspk",
    .init = pcspk_init,
    .play = pcspk_play,
    .stop = pcspk_stop,
};
