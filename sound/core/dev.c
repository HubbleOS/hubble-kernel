#include "dev.h"

#include <stddef.h>

static const struct sound_driver *active_driver = NULL;

int sound_register_driver(const struct sound_driver *drv)
{
	if (!drv || !drv->init)
		return -1;

	if (drv->init() != 0)
		return -1;

	active_driver = drv;
	return 0;
}

void sound_play(uint32_t freq, uint32_t duration_ms)
{
	if (active_driver && active_driver->play)
		active_driver->play(freq, duration_ms);
}

void sound_stop(void)
{
	if (active_driver && active_driver->stop)
		active_driver->stop();
}

void sound_init(void)
{
	// extern const struct sound_driver pcspk_driver;
	// sound_register_driver(&pcspk_driver);
	extern const struct sound_driver sb16_driver;
	sound_register_driver(&sb16_driver);
}
