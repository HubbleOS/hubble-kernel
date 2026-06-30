/**
 * @file dev.h
 * @brief Sound subsystem core interface
 */

#pragma once

#include <stdint.h>

/**
 * @brief Sound driver operations
 */
struct sound_driver
{
	const char *name;

	/** @brief Initialize the sound hardware */
	int (*init)(void);
	/** @brief Play a tone at a given frequency for a duration */
	void (*play)(uint32_t freq, uint32_t duration_ms);
	/** @brief Stop playback */
	void (*stop)(void);
};

/**
 * @brief Register a sound driver and initialize it
 * @param drv Pointer to the sound driver
 * @return 0 on success, -1 on error
 */
int sound_register_driver(const struct sound_driver *drv);

/**
 * @brief Play a tone through the active sound driver
 * @param freq Frequency in Hz
 * @param duration_ms Duration in milliseconds
 */
void sound_play(uint32_t freq, uint32_t duration_ms);

/**
 * @brief Stop playback on the active sound driver
 */
void sound_stop(void);
