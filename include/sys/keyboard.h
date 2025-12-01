// ============================================================================
// keyboard.h - Enhanced keyboard system with special keys support
// ============================================================================

#pragma once
#include <_cheader.h>
#include <stdint.h>
#include <stdbool.h>

// Basic key structure
typedef struct
{
	uint8_t scancode;
	bool extended;
} key_id_t;

// Keymap entry for character keys
typedef struct
{
	key_id_t id;
	char normal;
	char shifted;
} keymap_entry_t;

// Raw key event from keyboard
typedef struct
{
	key_id_t id;
	bool released;
	bool is_shift;
	bool is_ctrl;
	bool is_alt;
	bool is_caps_lock;
} key_event_t;

// ============================================================================
// Enhanced input system
// ============================================================================

// Key types for high-level processing
typedef enum
{
	KEY_TYPE_CHAR,	   // Printable character
	KEY_TYPE_SPECIAL,  // Arrow keys, navigation, etc.
	KEY_TYPE_FUNCTION, // F1-F12
	KEY_TYPE_MODIFIER, // Shift, Ctrl, Alt (usually skipped)
	KEY_TYPE_UNKNOWN   // Unknown key
} key_type_t;

// Special key actions
typedef enum
{
	KEY_ACTION_NONE = 0,
	KEY_ACTION_UP,
	KEY_ACTION_DOWN,
	KEY_ACTION_LEFT,
	KEY_ACTION_RIGHT,
	KEY_ACTION_HOME,
	KEY_ACTION_END,
	KEY_ACTION_INSERT,
	KEY_ACTION_DELETE,
	KEY_ACTION_PAGE_UP,
	KEY_ACTION_PAGE_DOWN,
	KEY_ACTION_BACKSPACE,
	KEY_ACTION_ENTER,
	KEY_ACTION_TAB,
	KEY_ACTION_ESC,
} key_action_t;

// High-level input event
typedef struct
{
	key_type_t type;
	union
	{
		char character;	      // For KEY_TYPE_CHAR
		key_action_t action;  // For KEY_TYPE_SPECIAL
		uint8_t function_key; // For KEY_TYPE_FUNCTION (1-12)
	};
	bool shift;
	bool ctrl;
	bool alt;
} input_event_t;

#include <gdt/interrupt.h>

_Begin_C_Header;

// Low-level keyboard functions (already exist)
key_event_t keyboard_get_event(void);
char keyboard_get_char(void);

// Enhanced high-level input function
input_event_t keyboard_get_input(void);

// Character mapping (already exists)
char keymap_lookup_char(uint8_t scancode, bool extended, bool shift, bool caps);

void keyboard_irq(registers_t *r);

_End_C_Header;
