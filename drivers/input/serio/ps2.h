/**
 * @file ps2.h
 * @brief PS/2 controller interface — data and command port I/O
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#define PS2_DATA 0x60
#define PS2_STATUS 0x64
#define PS2_COMMAND 0x64

#define PS2_STATUS_OUTPUT_FULL 0x01
#define PS2_STATUS_INPUT_FULL 0x02
#define PS2_STATUS_AUX_DATA 0x20

/**
 * @brief Initialise the PS/2 controller (enable translation and IRQs)
 * @return false if no controller responds; callers must then skip it
 */
bool ps2_init(void);

/**
 * @brief Wait until (status & mask) == want, at most timeout_ms
 */
bool ps2_wait_status(uint8_t mask, uint8_t want, uint64_t timeout_ms);

/**
 * @brief Send a byte to the command or data port once the controller is ready
 */
bool ps2_send(uint8_t port, uint8_t byte);

/**
 * @brief Read a byte from the data port
 * @return the byte, or -1 on timeout
 */
int ps2_recv(uint64_t timeout_ms);
