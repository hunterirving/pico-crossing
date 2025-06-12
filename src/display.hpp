#pragma once

#include "types.hpp"

void render_keyboard_state(KeyboardState* kb_state);
void render_controller_state(uint8_t* state);
void render_virtual_keyboard_state();
void render_device_section(DeviceState* device, int device_num);
void render_screen_update(DeviceState* device1, DeviceState* device2);
void render_timing_info();