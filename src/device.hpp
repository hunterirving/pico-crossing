#pragma once

#include "types.hpp"
#include "pico/stdlib.h"
#include "hardware/pio.h"

void controller_program_init(PIO pio, uint sm, uint offset, uint pin);
bool transfer(DeviceState* device, const uint8_t* request, uint8_t requestLength, 
			  uint8_t* response, uint8_t responseLength);
void init_device_state(DeviceState* device, PIO pio, uint pin);
bool detect_and_init_device(DeviceState* device);
uint8_t apply_calibration(uint8_t raw_value, int8_t offset);
void handle_standard_controller(DeviceState* device, int device_num);
void handle_keyboard_controller(DeviceState* device, int device_num);
