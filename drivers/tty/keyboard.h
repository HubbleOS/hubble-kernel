/**
 * @file keyboard.h
 * @brief TTY keyboard types — key events, keymap lookup, and input events
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Scancode-based key identifier
 */
typedef struct
{
	uint8_t scancode;
	bool extended;
} key_id_t;

/**
 * @brief Keymap entry mapping a key to its normal and shifted characters
 */
typedef struct
{
	key_id_t id;
	char normal;
	char shifted;
} keymap_entry_t;

/**
 * @brief Raw key event produced by the PS/2 handler
 */
typedef struct
{
	key_id_t id;
	bool released;
	bool is_shift;
	bool is_ctrl;
	bool is_alt;
	bool is_caps_lock;
} key_event_t;

/**
 * @brief High-level key type classification
 */
typedef enum
{
	KEY_TYPE_CHAR,
	KEY_TYPE_SPECIAL,
	KEY_TYPE_FUNCTION,
	KEY_TYPE_MODIFIER,
	KEY_TYPE_UNKNOWN
} key_type_t;

/**
 * @brief Special key action identifiers
 */
typedef enum
{
	KEY_ACTION_NONE,
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

/**
 * @brief High-level input event delivered to consumers
 */
typedef struct
{
	key_type_t type;
	union
	{
		char character;
		key_action_t action;
		uint8_t function_key;
	};
	bool shift;
	bool ctrl;
	bool alt;
} input_event_t;

/**
 * @brief Get the next keyboard event (blocking)
 * @return key_event_t The next available event
 */
key_event_t keyboard_get_event(void);

/**
 * @brief Non-blocking keyboard event poll
 * @param ev  Output event pointer
 * @return true if an event was available, false otherwise
 */
bool keyboard_poll_event(key_event_t *ev);

/**
 * @brief Look up the character for a given scancode + modifiers
 * @param scancode  Raw scancode
 * @param extended  Whether the 0xE0 prefix was received
 * @param shift     Shift held
 * @param caps      Caps lock active
 * @return The mapped character, or 0 if unmapped
 */
char keymap_lookup_char(uint8_t scancode, bool extended, bool shift, bool caps);

/**
 * @brief Block until a printable character is received
 * @return char The received character
 */
char keyboard_get_char(void);

/**
 * @brief Get a high-level processed input event (blocking)
 * @return input_event_t The processed event
 */
input_event_t keyboard_get_input(void);
