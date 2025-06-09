#pragma once

#include <stdint.h>
#include "hardware/pio.h"
#include <vector>

// Pin definitions
#define GPIO_OUTPUT_PIN 2
#define GPIO_INPUT_PIN_1 3
#define GPIO_INPUT_PIN_2 4

// Device and protocol constants
#define DEVICE_ID_CONTROLLER 0x0900
#define DEVICE_ID_KEYBOARD  0x0820
#define OVERFLOW_KEYCODE_1  0x01
#define OVERFLOW_KEYCODE_2  0x02
#define MOD_SHIFT          0x02
#define MOD_ALT            0x04

struct Utf8Char {
	uint8_t bytes[8];
	uint8_t length;
};

struct KeyTracker {
	uint8_t keycodes[256];
};

struct KeyboardState {
	bool caps_lock_pressed;
	uint8_t keycode1;
	uint8_t keycode2;
	uint8_t keycode3;
	uint8_t modifiers;
	bool in_overflow_state;
	uint8_t active_keys[3];
};

struct KeyMapping {
	const char* normal;
	const char* shift;
	const char* alt;
	const char* shift_alt;
};

struct DeviceState {
	PIO pio;
	uint sm;
	uint offset;
	uint pin;
	uint16_t device_id;
	bool initialized;
	bool is_keyboard;
	KeyboardState keyboard_state;
	KeyTracker key_tracker;
	uint8_t last_state[8];
	absolute_time_t next_retry_time;
	bool backspace_held;
	
	// Calibration offsets
	int8_t analog_x_offset;
	int8_t analog_y_offset;
	int8_t cstick_x_offset;
	int8_t cstick_y_offset;
	bool analog_calibrated;
};

struct VirtualKeyboardPos {
	uint8_t layer;
	uint8_t row;
	uint8_t col;

	bool operator==(const VirtualKeyboardPos& other) const {
		return layer == other.layer && row == other.row && col == other.col;
	}
	
	bool operator!=(const VirtualKeyboardPos& other) const {
		return !(*this == other);
	}
};

struct Utf8String {
	static bool equals(const char* str1, const char* str2) {
		if (!str1 || !str2) return false;
		while (*str1 && *str2) {
			int bytes1 = getUtf8ByteCount(*str1);
			int bytes2 = getUtf8ByteCount(*str2);
			
			if (bytes1 != bytes2) return false;
			
			for (int i = 0; i < bytes1; i++) {
				if (str1[i] != str2[i]) return false;
			}
			
			str1 += bytes1;
			str2 += bytes2;
		}
		return *str1 == *str2;
	}
	
	static int getUtf8ByteCount(unsigned char c) {
		if (c < 0x80) return 1;
		if ((c & 0xE0) == 0xC0) return 2;
		if ((c & 0xF0) == 0xE0) return 3;
		if ((c & 0xF8) == 0xF0) return 4;
		return 1;
	}
};

struct KeyBuffer {
	static const size_t BUFFER_SIZE = 256;
	Utf8Char buffer[BUFFER_SIZE];
	size_t readPos = 0;
	size_t writePos = 0;
	size_t count = 0;

	bool push(const Utf8Char& c);
	bool pop(Utf8Char& c);
	bool isEmpty() const;
	void clear() {
		readPos = 0;
		writePos = 0;
		count = 0;
	}
};

struct SimulatedState {
	uint8_t xStick;
	uint8_t yStick;
	uint32_t hold_duration_us;
	bool keyboard_calibrated;
};

extern SimulatedState simulatedState;
