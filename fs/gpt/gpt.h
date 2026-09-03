/* -- GPT partition table interface --------------------------------
 * Public API for GPT partition table parsing and access.
 * Exports partition-level information for higher-level filesystems.
 * ------------------------------------------------------------------ */

#pragma once

#include "gpt_struct.h"
#include <stdint.h>

/** @brief Initialize GPT partition table parsing.
 *
 * @param partitions  Array of gpt_partition_t to fill with discovered
 *                    partitions.
 * @param capacity    Maximum number of partitions the array can hold.
 * @return Number of partitions found, or -1 on error.
 */
int gpt_init(gpt_partition_t *partitions, uint32_t capacity);
