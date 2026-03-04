#pragma once

// Special keys + Helper macros
#define KEY_REALESE 0x80
#define KEY_EXTENDED 0xE0
#define KEY_SCANCODE_MASK 0x7F

#define IS_RELEASED(sc) (((sc) & KEY_REALESE) != 0)
#define IS_EXTENDED(sc) ((sc) == KEY_EXTENDED)
#define GET_SCANCODE(sc) ((sc) & KEY_SCANCODE_MASK)

// Control keys
#define KEY_ESC 0x01
#define KEY_BACKSPACE 0x0E
#define KEY_TAB 0x0F
#define KEY_ENTER 0x1C
#define KEY_SPACE 0x39
#define KEY_CAPS_LOCK 0x3A
#define KEY_SCROLL_LOCK 0x46
#define KEY_NUMLOCK 0x45
#define KEY_PAUSE 0xE1 0x1D 0x45 0xE1 0x9D 0xC5 // complex multi-byte scancode

// Modifier keys
#define KEY_LEFT_SHIFT 0x2A
#define KEY_RIGHT_SHIFT 0x36
#define KEY_LEFT_CTRL 0x1D
#define KEY_RIGHT_CTRL 0x1D // extended
#define KEY_LEFT_ALT 0x38
#define KEY_RIGHT_ALT 0x38 // extended

// Alphabetic keys
#define KEY_A 0x1E
#define KEY_B 0x30
#define KEY_C 0x2E
#define KEY_D 0x20
#define KEY_E 0x12
#define KEY_F 0x21
#define KEY_G 0x22
#define KEY_H 0x23
#define KEY_I 0x17
#define KEY_J 0x24
#define KEY_K 0x25
#define KEY_L 0x26
#define KEY_M 0x32
#define KEY_N 0x31
#define KEY_O 0x18
#define KEY_P 0x19
#define KEY_Q 0x10
#define KEY_R 0x13
#define KEY_S 0x1F
#define KEY_T 0x14
#define KEY_U 0x16
#define KEY_V 0x2F
#define KEY_W 0x11
#define KEY_X 0x2D
#define KEY_Y 0x15
#define KEY_Z 0x2C

// Numeric keys
#define KEY_1 0x02
#define KEY_2 0x03
#define KEY_3 0x04
#define KEY_4 0x05
#define KEY_5 0x06
#define KEY_6 0x07
#define KEY_7 0x08
#define KEY_8 0x09
#define KEY_9 0x0A
#define KEY_0 0x0B

// Symbol keys
#define KEY_MINUS 0x0C
#define KEY_EQUAL 0x0D
#define KEY_LEFT_BRACKET 0x1A
#define KEY_RIGHT_BRACKET 0x1B
#define KEY_SEMICOLON 0x27
#define KEY_APOSTROPHE 0x28
#define KEY_GRAVE 0x29
#define KEY_BACKSLASH 0x2B
#define KEY_COMMA 0x33
#define KEY_PERIOD 0x34
#define KEY_SLASH 0x35
#define KEY_ASTERISK 0x37

// Function keys
#define KEY_F1 0x3B
#define KEY_F2 0x3C
#define KEY_F3 0x3D
#define KEY_F4 0x3E
#define KEY_F5 0x3F
#define KEY_F6 0x40
#define KEY_F7 0x41
#define KEY_F8 0x42
#define KEY_F9 0x43
#define KEY_F10 0x44
#define KEY_F11 0x57
#define KEY_F12 0x58

// Numeric keypad
#define KEY_NUM_1 0x4F
#define KEY_NUM_2 0x50
#define KEY_NUM_3 0x51
#define KEY_NUM_4 0x4B
#define KEY_NUM_5 0x4C
#define KEY_NUM_6 0x4D
#define KEY_NUM_7 0x47
#define KEY_NUM_8 0x48
#define KEY_NUM_9 0x49
#define KEY_NUM_0 0x52
#define KEY_NUM_MINUS 0x4A
#define KEY_NUM_PLUS 0x4E
#define KEY_NUM_PERIOD 0x53
#define KEY_NUM_ENTER 0x1C // extended
#define KEY_NUM_SLASH 0x35 // extended

// Arrow keys
#define KEY_UP 0x48    // extended
#define KEY_LEFT 0x4B  // extended
#define KEY_RIGHT 0x4D // extended
#define KEY_DOWN 0x50  // extended

// Navigation keys
#define KEY_PAGEUP 0x49	   // extended
#define KEY_PAGEDOWN 0x51  // extended
#define KEY_HOME 0x32	   // extended
#define KEY_END 0x4F	   // 0xE0 0x4F
#define KEY_INSERT 0x52	   // extended
#define KEY_DELETE 0x53	   // extended
#define KEY_MENU 0x5D	   // extended
#define KEY_GUI_LEFT 0x5B  // extended
#define KEY_GUI_RIGHT 0x5C // extended
#define KEY_POWER 0x5E	   // extended
#define KEY_SLEEP 0x5F	   // extended
#define KEY_WAKE 0x63	   // extended

// Multimedia keys
#define KEY_PREV_TRACK 0x10  // extended
#define KEY_NEXT_TRACK 0x19  // extended
#define KEY_MUTE 0x20	     // 0xE0 0x20
#define KEY_CALCULATOR 0x21  // extended
#define KEY_PLAY 0x22	     // 0xE0 0x22
#define KEY_STOP 0x24	     // 0xE0 0x24
#define KEY_VOLUME_DOWN 0x2E // extended
#define KEY_VOLUME_UP 0x30   // extended

#define KEY_WWW_SEARCH 0x65    // extended
#define KEY_WWW_FAVORITES 0x66 // extended
#define KEY_WWW_REFRESH 0x67   // extended
#define KEY_WWW_STOP 0x68      // extended
#define KEY_WWW_FORWARD 0x69   // extended
#define KEY_WWW_BACK 0x6A      // extended

#define KEY_MY_COMPUTER 0x6B  // extended
#define KEY_MAIL 0x6C	      // extended
#define KEY_MEDIA_SELECT 0x6D // extended

// Other keys
#define KEY_PRTSC 0xE0 0x2A 0xE0 0x37 // special multi-byte sequence
