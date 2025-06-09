
#include "keymap.hpp"
#include <cstring>
#include <cctype>

KeyMapping keymap[256] = {{NULL, NULL, NULL, NULL}};
bool caps_lock_active = false;

const char* virtualKeyboard[4][4][10] = {
	{ // letters - lowercase qwerty
		{"!", "?", "\"", "-", "~", "–", "'", ";", ":", "🗝️"},
		{"q", "w", "e", "r", "t", "y", "u", "i", "o", "p"},
		{"a", "s", "d", "f", "g", "h", "j", "k", "l", "↵"},
		{"z", "x", "c", "v", "b", "n", "m", ",", ".", "␣"}
	},
	{ // letters - uppercase qwerty
		{"1", "2", "3", "4", "5", "6", "7", "8", "9", "0"},
		{"Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P"},
		{"A", "S", "D", "F", "G", "H", "J", "K", "L", "↵"},
		{"Z", "X", "C", "V", "B", "N", "M", ",", ".", "␣"}
	},
	{ // punctuation
		{"#", "?", "\"", "-", "~", "–", "·", ";", ":", "Æ"},
		{"%", "&", "@", "_", "‾", "/", "╏", "×", "÷", "="},
		{"(", ")", "<", ">", "»", "«", "≽", "≼", "+", "↵"},
		{"β", "þ", "ð", "§", "ǁ", "μ", "¬", ",", ".", "␣"}
	},
	{ // icons
		{"♥", "★", "♪", "💧", "💢", "🌺", "🐾", "♂", "♀", "∞"},
		{"⭕", "❌", "🔳", "🔺", "💀", "😱", "😁", "😞", "😡", "😀"},
		{"☀", "☁", "☂", "⛄", "🌀", "⚡", "🔨", "🎀", "✉", "↵"},
		{"🐿️", "🐱", "🐰", "🐙", "🐮", "🐷", "💰", "🐟", "🪲", "␣"}
	}
};

const char* get_key_name(uint8_t keycode) {
	switch (keycode) {
		case 0x50: return "BACKSPACE";
		case 0x61: return "ENTER";
		case 0x59: return "SPACE";
		case 0x53: return "CAPS LOCK";
		case 0x54: return "LEFT SHIFT";
		case 0x55: return "RIGHT SHIFT";
		case 0x57: return "ALT";
		case 0x00: return "NONE";
		default: return NULL;
	}
}

void init_keymap() {
	// Letters
	keymap[0x10] = {"a", "A", "a", "A"};
	keymap[0x11] = {"b", "B", "b", "B"};
	keymap[0x12] = {"c", "C", "c", "C"};
	keymap[0x13] = {"d", "D", "d", "D"};
	keymap[0x14] = {"e", "E", "e", "E"};
	keymap[0x15] = {"f", "F", "f", "F"};
	keymap[0x16] = {"g", "G", "g", "G"};
	keymap[0x17] = {"h", "H", "h", "H"};
	keymap[0x18] = {"i", "I", "i", "I"};
	keymap[0x19] = {"j", "J", "j", "J"};
	keymap[0x1A] = {"k", "K", "k", "K"};
	keymap[0x1B] = {"l", "L", "l", "L"};
	keymap[0x1C] = {"m", "M", "m", "M"};
	keymap[0x1D] = {"n", "N", "n", "N"};
	keymap[0x1E] = {"o", "O", "o", "O"};
	keymap[0x1F] = {"p", "P", "p", "P"};
	keymap[0x20] = {"q", "Q", "q", "Q"};
	keymap[0x21] = {"r", "R", "r", "R"};
	keymap[0x22] = {"s", "S", "s", "S"};
	keymap[0x23] = {"t", "T", "t", "T"};
	keymap[0x24] = {"u", "U", "u", "U"};
	keymap[0x25] = {"v", "V", "v", "V"};
	keymap[0x26] = {"w", "W", "w", "W"};
	keymap[0x27] = {"x", "X", "x", "X"};
	keymap[0x28] = {"y", "Y", "y", "Y"};
	keymap[0x29] = {"z", "Z", "z", "Z"};

	// Numbers row
	keymap[0x2A] = {"1", "!", "1", "!"};
	keymap[0x2B] = {"2", "@", "2", "@"};
	keymap[0x2C] = {"3", "#", "3", "#"};
	keymap[0x2D] = {"4", "§", "4", "§"};
	keymap[0x2E] = {"5", "%", "5", "%"};
	keymap[0x2F] = {"6", "&", "6", "&"};
	keymap[0x30] = {"7", "×", "7", "×"};
	keymap[0x31] = {"8", "÷", "8", "÷"};
	keymap[0x32] = {"9", "(", "9", "("};
	keymap[0x33] = {"0", ")", "0", ")"};

	// Special characters
	keymap[0x34] = {"-", "_", "-", "_"}; // hyphen, underscore
	keymap[0x35] = {"–", "‾", "–", "‾"}; // endash, overline
	keymap[0x36] = {"=", "+", "=", "+"};
	keymap[0x37] = {"β", "β", "β", "β"};
	keymap[0x38] = {"╏", "ǁ", "·", "·"};
	keymap[0x39] = {"Æ", "Æ", "Æ", "Æ"};
	keymap[0x3A] = {";", ":", ";", ":"};
	keymap[0x3B] = {"'", "\"", "'", "\""};
	keymap[0x3C] = {"μ", "μ", "μ", "μ"};
	keymap[0x3D] = {",", "<", ",", "<"};
	keymap[0x3E] = {".", ">", ".", ">"};
	keymap[0x3F] = {"/", "?", "/", "?"};

	keymap[0x4F] = {"¬", "~", "¬", "~"};

	keymap[0x51] = {"þ", "þ", "ð", "ð"};
	keymap[0x5A] = {"≽", "≽", "≼", "≼"};
	keymap[0x5B] = {"»", "»","«", "«"};

	// Top row // none, shift, alt, shift-alt
	keymap[0x4C] = {"🐮", "🐷", "🐰", "🐙"};
	keymap[0x40] = {"🐱", "🐿️", "💢", "🌺"};
	keymap[0x41] = {"♥", "★", "♪", "💧"};
	keymap[0x42] = {"☂", "☁", "⛄", "☀"};
	keymap[0x43] = {"🔨", "🎀", "🌀", "⚡"};
	keymap[0x44] = {"❌", "🔳", "⭕", "🔺"};
	keymap[0x45] = {"💰", "♀", "🐾", "♂"};
	keymap[0x46] = {"∞", "✉", "🐟", "🪲"};
	keymap[0x47] = {"😡", "😡", "😡", "😡"};
	keymap[0x48] = {"😞", "😞", "😞", "😞"};
	keymap[0x49] = {"😱", "😱", "😱", "😱"};
	keymap[0x4A] = {"😀", "😀", "😀", "😀"};
	keymap[0x4B] = {"😁", "😁", "😁", "😁"};
	keymap[0x4D] = {"💀", "💀", "💀", "💀"}; // Insert/ScrLk
	keymap[0x0A] = {"💀", "💀", "💀", "💀"}; // Fn + Insert/ScrLk
	keymap[0x61] = {"↵", "↵", "↵", "↵"};
	keymap[0x59] = {"␣", "␣", "␣", "␣"};

	// alt + shift for nook codes
	keymap[0x4E] = {"🗝️", "🗝️", "🗝️", "🔑"};

	// alt + shift for town tune
	keymap[0x56] = {"♪", "♪", "♪", "🐸"};

	// alt + shift for custom designs
	keymap[0x58] = {"🖌️", "🖌️", "🖌️", "🎨"};
}

Utf8Char translate_keycode(uint8_t keycode, uint8_t modifiers) {    
	Utf8Char result = {{0}, 0};
	const char* str = NULL;
	bool is_letter = (keycode >= 0x10 && keycode <= 0x29);
	
	if (is_letter) {
		bool shift_active = (modifiers & MOD_SHIFT) != 0;
		if (caps_lock_active) {
			shift_active = !shift_active;
		}
		if ((shift_active) && (modifiers & MOD_ALT)) {
			str = keymap[keycode].shift_alt;
		} else if (shift_active) {
			str = keymap[keycode].shift;
		} else if (modifiers & MOD_ALT) {
			str = keymap[keycode].alt;
		} else {
			str = keymap[keycode].normal;
		}
	} else {
		if ((modifiers & (MOD_SHIFT | MOD_ALT)) == (MOD_SHIFT | MOD_ALT)) {
			str = keymap[keycode].shift_alt;
		} else if (modifiers & MOD_SHIFT) {
			str = keymap[keycode].shift;
		} else if (modifiers & MOD_ALT) {
			str = keymap[keycode].alt;
		} else {
			str = keymap[keycode].normal;
		}
	}
	
	if (str) {
		size_t i = 0;
		while (str[i] && i < 8) {
			result.bytes[i] = str[i];
			i++;
		}
		result.length = i;
	}
	
	return result;
}
