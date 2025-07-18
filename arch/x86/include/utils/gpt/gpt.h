#pragma once
#include <stdint.h>
#include "utils/gpt/gpt_struct.h"
extern uint32_t first_usable_lba;
extern uint32_t last_usable_lba;

int gpt_init(gpt_partition_t *partitions);