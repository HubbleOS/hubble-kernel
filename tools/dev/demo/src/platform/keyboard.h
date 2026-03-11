#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <gui/dev/keyboard/keyboard.h>
#include <SDL2/SDL.h>

// Эмуляция key_event_t через SDL
bool keyboard_poll_sdl(input_event_t *ev);

// Маппинг SDL key -> keymap_lookup_char
char keymap_lookup_sdl(SDL_Keysym keysym, bool shift, bool caps);
