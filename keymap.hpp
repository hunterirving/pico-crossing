#pragma once

#include "types.hpp"

void init_keymap();
const char* get_key_name(uint8_t keycode);
Utf8Char translate_keycode(uint8_t keycode, uint8_t modifiers);

extern KeyMapping keymap[256];
extern bool caps_lock_active;

extern const char* virtualKeyboard[4][4][10];