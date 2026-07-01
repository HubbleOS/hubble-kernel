#pragma once

/**
 * @brief Generic device abstraction.
 *
 * Devices are registered during boot by drivers and can be looked up
 * by name or type from the kernel side.
 */

#include <stdint.h>

#define DEV_SOUND 1
#define DEV_NET 2
#define DEV_OTHER 3

struct device {
  const char *name; /**< Human-readable device name. */
  uint32_t type;    /**< One of DEV_*.              */
  void *ops;        /**< Driver operations struct.   */
  void *priv;       /**< Driver-private data.        */
};

void device_register(struct device *dev);
struct device *device_find_by_name(const char *name);
struct device *device_find_by_type(uint32_t type);
