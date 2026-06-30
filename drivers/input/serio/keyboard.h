/**
 * @file keyboard.h
 * @brief PS/2 keyboard driver — IRQ handler and device registration
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <interrupt/interrupt.h>

/**
 * @brief PS/2 keyboard IRQ handler
 * @param r Register state at the time of the interrupt
 */
void keyboard_irq(registers_t *r);
