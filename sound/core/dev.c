/**
 * @file dev.c
 * @brief Sound subsystem core: driver registration and playback control
 */

#include <stddef.h>
#include <hubble/printk.h>
#include "dev.h"

static const struct sound_driver *active_driver = NULL;

/**
 * @brief Register and initialize a sound driver
 * @param drv Pointer to the sound driver
 * @return 0 on success, -1 on error (driver must have an init function)
 */
int sound_register_driver(const struct sound_driver *drv)
{
	if (!drv || !drv->init)
		return -1;

	if (drv->init() != 0)
		return -1;

	printk(KERN_INFO "[sound] driver %s registered\n", drv->name);

	active_driver = drv;
	return 0;
}

/**
 * @brief Play a tone through the active sound driver
 * @param freq Frequency in Hz
 * @param duration_ms Duration in milliseconds
 */
void sound_play(uint32_t freq, uint32_t duration_ms)
{
	if (active_driver && active_driver->play)
		active_driver->play(freq, duration_ms);
}

/**
 * @brief Stop playback on the active sound driver
 */
void sound_stop(void)
{
	if (active_driver && active_driver->stop)
		active_driver->stop();
}
