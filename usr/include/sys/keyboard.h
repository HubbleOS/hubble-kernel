#pragma once

#include <_cheader.h>

#include <stdint.h>
#include <stdbool.h>

typedef struct
{
	uint8_t scancode;
	bool extended;
} key_id_t;

typedef struct
{
	key_id_t id;
	char normal;
	char shifted;
} keymap_entry_t;

typedef struct
{
	key_id_t id;
	bool released;
	bool is_shift;
	bool is_ctrl;
	bool is_alt;
	bool is_caps_lock;
} key_event_t;

_Begin_C_Header;

key_event_t read_key_event();

char keymap_lookup_char(uint8_t scancode, bool extended, bool shift, bool caps);

_End_C_Header;
