/* ── GPT partition table interface ────────────────────────────────
 * Public API for GPT partition table parsing and access.
 * Exports partition-level information for higher-level filesystems.
 * ────────────────────────────────────────────────────────────────── */

#pragma once

#include <stdint.h>
#include "gpt_struct.h"

extern uint32_t first_usable_lba;
extern uint32_t last_usable_lba;

/** @brief Initialize GPT partition table parsing.
 *
 * @param partitions  Array of gpt_partition_t to fill with discovered partitions.
 * @return Number of partitions found, or -1 on error.
 */
int gpt_init(gpt_partition_t *partitions);
