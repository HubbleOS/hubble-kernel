/**
 * @file ps2.h
 * @brief PS/2 controller interface — data and command port I/O
 */
#pragma once

#include <stdint.h>

#define PS2_DATA    0x60
#define PS2_STATUS  0x64
#define PS2_COMMAND 0x64

/**
 * @brief Initialise the PS/2 controller (disable translation, enable IRQs)
 */
void ps2_init(void);

/**
 * @brief Wait until the controller is ready to accept a command / data byte
 */
void ps2_wait_input(void);

/**
 * @brief Wait until the controller has output data available to read
 */
void ps2_wait_output(void);
