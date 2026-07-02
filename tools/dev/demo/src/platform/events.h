/**
 * @file events.h
 * @brief  Event processing and input handling
 */
#pragma once
#include <SDL2/SDL.h>

/**
 * @brief Process SDL events and update input state.
 *
 * Polls SDL event queue and updates the global mouse state.
 * Handles window close events and basic mouse input.
 *
 * @return int
 * @retval 1 Continue application loop
 * @retval 0 Exit application
 */
int events_process(void);
