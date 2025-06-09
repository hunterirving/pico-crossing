
#include "display.hpp"
#include "keymap.hpp"
#include <stdio.h>

extern const char* virtualKeyboard[4][4][10];
extern VirtualKeyboardPos currentPos;
extern KeyBuffer keyBuffer;
extern SimulatedState simulatedState;

void render_keyboard_state(KeyboardState* kb_state) {
	printf("Type: Keyboard\n\x1B[K");
	
	if (kb_state->in_overflow_state) {
		printf("Modifiers: None\n\x1B[K");
		printf("Active keys: Too many keys pressed!\n\x1B[K");
		return;
	}

	printf("Modifiers: %s%s%s\n\x1B[K",
		(kb_state->modifiers & MOD_SHIFT) ? "SHIFT " : "      ",
		(kb_state->modifiers & MOD_ALT) ? "ALT " : "    ",
		caps_lock_active ? "CAPS " : "     ");
	
	printf("Active keys: ");
	bool has_active_keys = false;
	uint8_t keycodes[] = {kb_state->keycode1, kb_state->keycode2, kb_state->keycode3};
	
	for (int i = 0; i < 3; i++) {
		uint8_t keycode = keycodes[i];
		if (keycode != 0 && keycode != 0x54 && keycode != 0x55 && keycode != 0x57 && keycode != 0x53) {
			has_active_keys = true;
			Utf8Char c = translate_keycode(keycode, kb_state->modifiers);
			if (c.length > 0) {
				printf("[ ");
				for (int j = 0; j < c.length; j++) {
					putchar(c.bytes[j]);
				}
				printf(" ] ");
			} else {
				const char* key_name = get_key_name(keycode);
				if (key_name) {
					printf("[ %s ] ", key_name);
				} else {
					printf("[0x%02X] ", keycode);
				}
			}
		}
	}
	if (!has_active_keys) {
		printf("None");
	}
	printf("\n\x1B[K");
}

void render_controller_state(uint8_t* state) {
	printf("Type: Controller\n\x1B[K");
	printf("├─ Buttons\n\x1B[K");
	printf("│  ├─ A: [%s]  B: [%s]  X: [%s]  Y: [%s]\n\x1B[K",
		(state[0] & 0x01) ? "X" : " ",
		(state[0] & 0x02) ? "X" : " ",
		(state[0] & 0x04) ? "X" : " ",
		(state[0] & 0x08) ? "X" : " ");
	printf("│  ├─ Start: [%s]  Z: [%s]\n\x1B[K",
		(state[0] & 0x10) ? "X" : " ",
		(state[1] & 0x10) ? "X" : " ");
	printf("│  └─ L: [%s]  R: [%s]\n\x1B[K",
		(state[1] & 0x40) ? "X" : " ",
		(state[1] & 0x20) ? "X" : " ");
	printf("├─ Analog\n\x1B[K");
	printf("│  ├─ Main Stick:  X: %4d  Y: %4d\n\x1B[K",
		state[2], state[3]);
	printf("│  ├─ C-Stick:     X: %4d  Y: %4d\n\x1B[K",
		state[4], state[5]);
	printf("│  └─ Triggers:    L: %4d  R: %4d\n\x1B[K",
		state[6], state[7]);
	printf("└─ D-Pad: ");
	
	switch (state[1] & 0x0F) {
		case 0x00: printf("Neutral   \n\x1B[K"); break;
		case 0x01: printf("Left      \n\x1B[K"); break;
		case 0x02: printf("Right     \n\x1B[K"); break;
		case 0x04: printf("Down      \n\x1B[K"); break;
		case 0x08: printf("Up        \n\x1B[K"); break;
		case 0x05: printf("Down-Left \n\x1B[K"); break;
		case 0x09: printf("Up-Left   \n\x1B[K"); break;
		case 0x06: printf("Down-Right\n\x1B[K"); break;
		case 0x0A: printf("Up-Right  \n\x1B[K"); break;
		default:   printf("Invalid   \n\x1B[K");
	}
}

void render_virtual_keyboard_state() {
	printf("=== Virtual Keyboard State ===\n\x1B[K");
	printf("Calibration Status: %s\n\x1B[K", 
		   simulatedState.keyboard_calibrated ? "Calibrated" : "Uncalibrated");
	printf("Current Position: Layer %d, Row %d, Col %d\n\x1B[K", 
		   currentPos.layer, currentPos.row, currentPos.col);
	printf("Key Buffer [%zu/%zu]: ", keyBuffer.count, keyBuffer.BUFFER_SIZE);
	if (keyBuffer.count > 0) {
		size_t current = keyBuffer.readPos;
		for (size_t i = 0; i < keyBuffer.count; i++) {
			printf("%c ", keyBuffer.buffer[current]);
			current = (current + 1) % keyBuffer.BUFFER_SIZE;
		}
	} else {
		printf("empty");
	}
	printf("\n\x1B[K");
	
	printf("Current Layer Layout:\n\x1B[K");
	for (int row = 0; row < 4; row++) {
		printf("  ");
		for (int col = 0; col < 10; col++) {
			if (row == currentPos.row && col == currentPos.col) {
				if (simulatedState.keyboard_calibrated) {
					printf("[%s]", virtualKeyboard[currentPos.layer][row][col]);
				} else {
					printf(" %s ", virtualKeyboard[currentPos.layer][row][col]);
				}
			} else {
				printf(" %s ", virtualKeyboard[currentPos.layer][row][col]);
			}
		}
		printf("\n\x1B[K");
	}
	printf("\n\x1B[K");
}

void render_device_section(DeviceState* device, int device_num) {
	printf("=== Device %d ===\n\x1B[K", device_num);
	if (device->initialized) {
		if (device->is_keyboard) {
			render_keyboard_state(&device->keyboard_state);
		} else {
			render_controller_state(device->last_state);
		}
	} else {
		printf("No device detected\n\x1B[K");
	}
	printf("\n\x1B[K");
}

void render_screen_update(DeviceState* device1, DeviceState* device2) {
	printf("\x1B[H\x1B[K");
	render_device_section(device1, 1);
	render_device_section(device2, 2);
	render_virtual_keyboard_state();
	fflush(stdout);
}
