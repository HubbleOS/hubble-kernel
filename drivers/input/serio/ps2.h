#pragma once

#include <stdint.h>

#define PS2_DATA 0x60
#define PS2_STATUS 0x64
#define PS2_COMMAND 0x64

void ps2_init(void);
void ps2_wait_input(void);
void ps2_wait_output(void);
