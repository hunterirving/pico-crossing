#include "device.hpp"
#include "simulatedController.hpp"
#include "keymap.hpp"
#include "townTunes.hpp"
#include "controller.pio.h"
#include "hardware/clocks.h"
#include <cstdio>
#include <cstring>

#define RETRY_DELAY_MS 500

// External declarations
extern bool caps_lock_active;
extern KeyBuffer keyBuffer;

// Command sequences
const uint8_t CMD_RESET[] = {0x00};
const uint8_t CMD_KEYBOARD_POLL[] = {0x54, 0x00, 0x00};
const uint8_t CMD_STANDARD_POLL[] = {0x40, 0x03, 0x00};
const uint8_t CMD_ORIGIN[] = {0x41, 0x00, 0x00};

void controller_program_init(PIO pio, uint sm, uint offset, uint pin) {
	pio_sm_config c = controller_program_get_default_config(offset);
	pio_sm_set_consecutive_pindirs(pio, sm, pin, 1, false);
	pio_gpio_init(pio, pin);

	sm_config_set_in_pins(&c, pin);
	sm_config_set_sideset_pins(&c, pin);
	sm_config_set_jmp_pin(&c, pin);
	sm_config_set_out_shift(&c, false, true, 8);
	sm_config_set_in_shift(&c, false, true, 8);

	int cycles_per_bit = (controller_T1 + controller_T2) / 4;
	float frequency = 1000000;
	float div = clock_get_hz(clk_sys) / (cycles_per_bit * frequency);
	sm_config_set_clkdiv(&c, div);

	pio_sm_init(pio, sm, offset, &c);
	pio_sm_set_enabled(pio, sm, true);
}

bool transfer(DeviceState* device, const uint8_t* request, uint8_t requestLength, 
			  uint8_t* response, uint8_t responseLength) {
	pio_sm_clear_fifos(device->pio, device->sm);
	pio_sm_put_blocking(device->pio, device->sm, ((responseLength - 1) & 0x1F) << 24);
	
	for (int i = 0; i < requestLength; i++) {
		pio_sm_put_blocking(device->pio, device->sm, request[i] << 24);
	}
	
	bool timeout = false;
	for (int i = 0; i < responseLength; i++) {
		absolute_time_t timeout_us = make_timeout_time_us(2000);
		while (pio_sm_is_rx_fifo_empty(device->pio, device->sm)) {
			if (time_reached(timeout_us)) {
				timeout = true;
				break;
			}
		}
		if (timeout) break;
		uint32_t data = pio_sm_get(device->pio, device->sm);
		response[i] = (uint8_t)(data & 0xFF);
	}
	
	busy_wait_us(8 * (requestLength + responseLength) + 900);
	return !timeout;
}

void tracker_init(KeyTracker* tracker) {
	memset(tracker->keycodes, 0, sizeof(tracker->keycodes));
}

bool tracker_is_active(KeyTracker* tracker, uint8_t keycode) {
	return tracker->keycodes[keycode] > 0;
}

void tracker_add_key(KeyTracker* tracker, uint8_t keycode) {
	if (!tracker_is_active(tracker, keycode) && keycode != 0) {
		tracker->keycodes[keycode] = 1;
	}
}

void tracker_remove_key(KeyTracker* tracker, uint8_t keycode) {
	if (tracker_is_active(tracker, keycode)) {
		tracker->keycodes[keycode] = 0;
	}
}

void init_device_state(DeviceState* device, PIO pio, uint pin) {
	device->pio = pio;
	device->sm = pio_claim_unused_sm(pio, true);
	
	static uint loaded_offset = 0;
	static bool program_loaded = false;
	if (!program_loaded) {
		loaded_offset = pio_add_program(pio, &controller_program);
		program_loaded = true;
	}
	device->offset = loaded_offset;
	
	device->pin = pin;
	device->device_id = 0;
	device->initialized = false;
	device->is_keyboard = false;
	memset(&device->keyboard_state, 0, sizeof(KeyboardState));
	tracker_init(&device->key_tracker);
	
	// Initialize calibration values
	device->analog_x_offset = 0;
	device->analog_y_offset = 0;
	device->cstick_x_offset = 0;
	device->cstick_y_offset = 0;
	device->analog_calibrated = false;
}

bool detect_and_init_device(DeviceState* device) {
	absolute_time_t now = get_absolute_time();

	if (absolute_time_diff_us(device->next_retry_time, now) > 0) {
		pio_sm_set_enabled(device->pio, device->sm, false);
		pio_sm_clear_fifos(device->pio, device->sm);

		controller_program_init(device->pio, device->sm, device->offset, device->pin);
		sleep_ms(100);

		uint8_t response[8];
		if (!transfer(device, CMD_RESET, sizeof(CMD_RESET), response, 3)) {
			device->next_retry_time = make_timeout_time_ms(RETRY_DELAY_MS);
			return false;
		}

		uint16_t device_id = (response[0] << 8) | response[1];
		if (device_id == DEVICE_ID_KEYBOARD) {
			device->is_keyboard = true;
			device->device_id = device_id;
			device->initialized = true;
			return true;
		} else if (device_id == DEVICE_ID_CONTROLLER) {
			device->is_keyboard = false;
			device->device_id = device_id;
			device->initialized = true;
			return true;
		}

		device->next_retry_time = make_timeout_time_ms(RETRY_DELAY_MS);
		return false;
	}

	return device->initialized;
}

uint8_t apply_calibration(uint8_t raw_value, int8_t offset) {
	int16_t calibrated = raw_value + offset;
	if (calibrated > 255) return 255;
	if (calibrated < 0) return 0;
	return (uint8_t)calibrated;
}

void handle_standard_controller(DeviceState* device, int device_num) {
	uint8_t response[8];
	
	if (!transfer(device, CMD_STANDARD_POLL, sizeof(CMD_STANDARD_POLL), response, 8)) {
		device->initialized = false;
		return;
	}
	
	// If not calibrated yet, calculate offsets
	if (!device->analog_calibrated) {
		device->analog_x_offset = 128 - response[2];
		device->analog_y_offset = 128 - response[3];
		device->cstick_x_offset = 128 - response[4];
		device->cstick_y_offset = 128 - response[5];
		device->analog_calibrated = true;
	}
	
	// Apply calibration to the raw values with bounds checking
	response[2] = apply_calibration(response[2], device->analog_x_offset);
	response[3] = apply_calibration(response[3], device->analog_y_offset);
	response[4] = apply_calibration(response[4], device->cstick_x_offset);
	response[5] = apply_calibration(response[5], device->cstick_y_offset);
	
	memcpy(device->last_state, response, 8);
}

void parse_keyboard_data(uint8_t *data, KeyboardState *state) {    
	state->keycode1 = data[4];
	state->keycode2 = data[5];
	state->keycode3 = data[6];

	bool overflow_condition = 
		(state->keycode1 == OVERFLOW_KEYCODE_1 || state->keycode1 == OVERFLOW_KEYCODE_2) &&
		(state->keycode2 == OVERFLOW_KEYCODE_1 || state->keycode2 == OVERFLOW_KEYCODE_2) &&
		(state->keycode3 == OVERFLOW_KEYCODE_1 || state->keycode3 == OVERFLOW_KEYCODE_2);

	bool modifier_only_condition = 
		(state->keycode1 == 0x00 || state->keycode1 == 0x54 || state->keycode1 == 0x55 || state->keycode1 == 0x57) &&
		(state->keycode2 == 0x00 || state->keycode2 == 0x54 || state->keycode2 == 0x55 || state->keycode2 == 0x57) &&
		(state->keycode3 == 0x00 || state->keycode3 == 0x54 || state->keycode3 == 0x55 || state->keycode3 == 0x57);

	state->in_overflow_state = overflow_condition && !modifier_only_condition;
	
	state->modifiers = 0;
	uint8_t keycodes[] = {state->keycode1, state->keycode2, state->keycode3};
	
	for (int i = 0; i < 3; i++) {
		switch (keycodes[i]) {
			case 0x54:
			case 0x55:
				state->modifiers |= MOD_SHIFT;
				break;
			case 0x57:
				state->modifiers |= MOD_ALT;
				break;
		}
	}

	bool caps_found = false;
	for (int i = 0; i < 3; i++) {
		if (keycodes[i] == 0x53) {
			caps_found = true;
			break;
		}
	}

	if (caps_found && !state->caps_lock_pressed) {
		caps_lock_active = !caps_lock_active;
	}
	state->caps_lock_pressed = caps_found;
}

void handle_keyboard_controller(DeviceState* device, int device_num) {
	uint8_t response[8];
	static KeyTracker lastKeyTracker;
	
	if (!transfer(device, CMD_KEYBOARD_POLL, sizeof(CMD_KEYBOARD_POLL), response, 8)) {
		device->initialized = false;
		return;
	}
	
	parse_keyboard_data(response, &device->keyboard_state);
	
	device->backspace_held = false;
	uint8_t keycodes[] = {device->keyboard_state.keycode1, 
						 device->keyboard_state.keycode2, 
						 device->keyboard_state.keycode3};
	
	for (int i = 0; i < 3; i++) {
		if (keycodes[i] == 0x50) {
			device->backspace_held = true;
			break;
		}
	}
	
	// Check town tune mode status
	bool in_town_tune_mode = TownTunes::isInTownTuneMode();
	
	for (int i = 0; i < 3; i++) {
		if (keycodes[i] != 0 && keycodes[i] != 0x50) {
			if (!tracker_is_active(&lastKeyTracker, keycodes[i])) {
				Utf8Char c = translate_keycode(keycodes[i], device->keyboard_state.modifiers);
				// Only add to keyBuffer if we're not in town tune mode
				if (c.length > 0 && !in_town_tune_mode) {
					keyBuffer.push(c);
				}
			}
			tracker_add_key(&device->key_tracker, keycodes[i]);
			tracker_add_key(&lastKeyTracker, keycodes[i]);
		}
	}
	
	for (int i = 0; i < 256; i++) {
		bool key_still_pressed = false;
		for (int j = 0; j < 3; j++) {
			if (keycodes[j] == i) {
				key_still_pressed = true;
				break;
			}
		}
		if (!key_still_pressed && tracker_is_active(&lastKeyTracker, i)) {
			tracker_remove_key(&lastKeyTracker, i);
			tracker_remove_key(&device->key_tracker, i); // Also remove from device's key tracker
		}
	}
}
