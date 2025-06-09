#pragma once

#include "types.hpp"
#include "gcReport.hpp"

// Process keyboard buffer and controller inputs to generate GC controller report
void processKeyBuffer(GCReport& report, bool bPressed, uint8_t dpadState, uint8_t buttons1, uint8_t buttons2,
					 uint8_t analogX, uint8_t analogY, uint8_t cX, uint8_t cY);

// Find all positions of a character on the virtual keyboard
std::vector<VirtualKeyboardPos> findCharacterPositions(char targetChar);

// Find the closest position to current position from a list of positions
VirtualKeyboardPos findClosestPosition(const std::vector<VirtualKeyboardPos>& positions);

// Get the final state of the simulated controller
GCReport getControllerState();

// Calculate distance between two positions on virtual keyboard
int calculateDistance(const VirtualKeyboardPos& from, const VirtualKeyboardPos& to);