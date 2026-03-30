#pragma once

#include <stdint.h>

struct sound_driver
{
	const char *name;

	int (*init)(void);
	void (*play)(uint32_t freq, uint32_t duration_ms);
	void (*stop)(void);
};

int sound_register_driver(const struct sound_driver *drv);
void sound_play(uint32_t freq, uint32_t duration_ms);
void sound_stop(void);
void sound_init(void);
