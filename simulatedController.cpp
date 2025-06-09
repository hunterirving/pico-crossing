// simulatedController.cpp
#include "simulatedController.hpp"
#include "keyboardCalibration.hpp"
#include "nookCodes.hpp"
#include "townTunes.hpp"
#include "design.hpp"
#include "snake.hpp"
#include "types.hpp"
#include <cstdio>
#include <cstdlib>
#include <cctype>
#include <string>
#include <map>
#include <cstring>
#include <vector>
#include <queue>

// External declarations for key tracking functions
extern bool tracker_is_active(KeyTracker* tracker, uint8_t keycode);

// External declarations for accessing design mode variables
extern uint8_t currentPalette;
extern uint8_t currentColor;

// for Snake High Scores
std::queue<uint8_t> initialKeyCodeBuffer;
static std::map<uint8_t, bool> initialKeyState;

bool isEmptyChar(const Utf8Char& c) {
	return c.length == 0;
}

Utf8Char getEmptyChar() {
	return {{0}, 0};
}

extern DeviceState device1;
extern DeviceState device2;
extern KeyBuffer keyBuffer;
VirtualKeyboardPos currentPos = {0, 0, 0};
static VirtualKeyboardPos targetPos = {0, 0, 0};
extern const char* virtualKeyboard[4][4][10];
static bool needsBackspace = false;

SimulatedState simulatedState = {
	.xStick = 128,
	.yStick = 128,
	.hold_duration_us = 17000,
	.keyboard_calibrated = false,
};

std::vector<VirtualKeyboardPos> findCharacterPositions(const Utf8Char& targetChar) {
	std::vector<VirtualKeyboardPos> positions;
	
	// First look in current layer
	for (uint8_t row = 0; row < 4; row++) {
		for (uint8_t col = 0; col < 10; col++) {
			const char* cellChar = virtualKeyboard[currentPos.layer][row][col];
			bool match = true;
			for (size_t i = 0; i < targetChar.length; i++) {
				if (cellChar[i] != targetChar.bytes[i] || cellChar[i] == 0) {
					match = false;
					break;
				}
			}
			if (match && cellChar[targetChar.length] == 0) {
				positions.push_back({currentPos.layer, row, col});
				return positions; // Return immediately if found in current layer
			}
		}
	}
	
	// If not found in current layer, search all layers
	for (uint8_t layer = 0; layer < 4; layer++) {
		if (layer == currentPos.layer) continue; // Skip current layer as we already checked it
		
		for (uint8_t row = 0; row < 4; row++) {
			for (uint8_t col = 0; col < 10; col++) {
				const char* cellChar = virtualKeyboard[layer][row][col];
				bool match = true;
				for (size_t i = 0; i < targetChar.length; i++) {
					if (cellChar[i] != targetChar.bytes[i] || cellChar[i] == 0) {
						match = false;
						break;
					}
				}
				if (match && cellChar[targetChar.length] == 0) {
					positions.push_back({layer, row, col});
				}
			}
		}
	}
	
	return positions;
}

int calculateDistance(const VirtualKeyboardPos& from, const VirtualKeyboardPos& to) {
	return abs(to.row - from.row) + abs(to.col - from.col);
}

VirtualKeyboardPos findClosestPosition(const std::vector<VirtualKeyboardPos>& positions) {
	if (positions.empty()) return currentPos;
	
	VirtualKeyboardPos closest = positions[0];
	int minDistance = calculateDistance(currentPos, closest);
	
	for (const auto& pos : positions) {
		int distance = calculateDistance(currentPos, pos);
		if (distance < minDistance) {
			minDistance = distance;
			closest = pos;
		}
	}
	
	return closest;
}

bool isAnalogOutsideDeadzone(uint8_t analogX, uint8_t analogY) {
	const int DEADZONE = 30;
	const int CENTER = 128;
	
	int deltaX = analogX - CENTER;
	int deltaY = analogY - CENTER;
	
	// Check if outside circular deadzone
	return (deltaX * deltaX + deltaY * deltaY) > (DEADZONE * DEADZONE);
}

void handlePassthrough(GCReport& report, uint8_t buttons1, uint8_t buttons2, uint8_t dpadState,
					  uint8_t analogX, uint8_t analogY, uint8_t cX, uint8_t cY) {
	static bool lastLState = false;
	static bool lastYState = false;
	static uint8_t arrowKeyDpad = 0; // Tracks D-Pad state from arrow keys

	// Handle L button state changes
	bool currentLState = (buttons2 & 0x40) != 0;
	if (currentLState && !lastLState) {
		if (currentPos.layer <= 1) {
			currentPos.layer = (currentPos.layer == 0) ? 1 : 0;
		}
	}
	lastLState = currentLState;

	// Handle Y button state changes
	bool currentYState = (buttons1 & 0x08) != 0;
	if (currentYState && !lastYState) {
		if (currentPos.layer <= 1) {
			currentPos.layer = 2;
		} else if (currentPos.layer == 2) {
			currentPos.layer = 3;
		} else {
			currentPos.layer = 0;
		}
	}
	lastYState = currentYState;

	// Check for analog stick movement outside deadzone
	if (isAnalogOutsideDeadzone(analogX, analogY)) {
		simulatedState.keyboard_calibrated = false;
	}

	// Check for START button press
	if (buttons1 & 0x10) { 
		simulatedState.keyboard_calibrated = false;
	}

	// Handle arrow key inputs from attached keyboard
	arrowKeyDpad = 0;
	
	// Check for arrow key activations in devices
	if (device1.initialized && device1.is_keyboard) {
		// Left arrow (0x5C) or Fn+Left (0x08)
		if (tracker_is_active(&device1.key_tracker, 0x5C) || tracker_is_active(&device1.key_tracker, 0x08)) {
			arrowKeyDpad |= 0x01; // D-Left
		}
		// Down arrow (0x5D) or Fn+Down (0x09)
		if (tracker_is_active(&device1.key_tracker, 0x5D) || tracker_is_active(&device1.key_tracker, 0x09)) {
			arrowKeyDpad |= 0x04; // D-Down
		}
		// Up arrow (0x5E) or Fn+Up (0x06)
		if (tracker_is_active(&device1.key_tracker, 0x5E) || tracker_is_active(&device1.key_tracker, 0x06)) {
			arrowKeyDpad |= 0x08; // D-Up
		}
		// Right arrow (0x5F) or Fn+Right (0x07)
		if (tracker_is_active(&device1.key_tracker, 0x5F) || tracker_is_active(&device1.key_tracker, 0x07)) {
			arrowKeyDpad |= 0x02; // D-Right
		}
	}
	
	if (device2.initialized && device2.is_keyboard) {
		// Left arrow (0x5C)
		if (tracker_is_active(&device2.key_tracker, 0x5C)) {
			arrowKeyDpad |= 0x01; // D-Left
		}
		// Down arrow (0x5D)
		if (tracker_is_active(&device2.key_tracker, 0x5D)) {
			arrowKeyDpad |= 0x04; // D-Down
		}
		// Up arrow (0x5E)
		if (tracker_is_active(&device2.key_tracker, 0x5E)) {
			arrowKeyDpad |= 0x08; // D-Up
		}
		// Right arrow (0x5F)
		if (tracker_is_active(&device2.key_tracker, 0x5F)) {
			arrowKeyDpad |= 0x02; // D-Right
		}
	}

	// Pass through all inputs
	report.xStick = analogX;
	report.yStick = analogY;
	report.cxStick = cX;
	report.cyStick = cY;
	
	// Combine controller D-Pad with arrow key inputs
	report.dLeft = ((dpadState & 0x01) || (arrowKeyDpad & 0x01)) ? 1 : 0;
	report.dRight = ((dpadState & 0x02) || (arrowKeyDpad & 0x02)) ? 1 : 0;
	report.dDown = ((dpadState & 0x04) || (arrowKeyDpad & 0x04)) ? 1 : 0;
	report.dUp = ((dpadState & 0x08) || (arrowKeyDpad & 0x08)) ? 1 : 0;
	
	report.a = (buttons1 & 0x01) ? 1 : 0;
	report.b = (buttons1 & 0x02) ? 1 : 0;
	report.x = (buttons1 & 0x04) ? 1 : 0;
	report.y = (buttons1 & 0x08) ? 1 : 0;
	report.start = (buttons1 & 0x10) ? 1 : 0;
	
	report.z = (buttons2 & 0x10) ? 1 : 0;
	
	if (buttons2 & 0x40) {
		report.l = 1;
		report.analogL = 255;
	}
	if (buttons2 & 0x20) {
		report.r = 1;
		report.analogR = 255;
	}
}

void processKeyBuffer(GCReport& report, bool bPressed, uint8_t dpadState, uint8_t buttons1, uint8_t buttons2, 
					uint8_t analogX, uint8_t analogY, uint8_t cX, uint8_t cY) {
	static enum class State {
		IDLE,
		CALIBRATING,
		NEUTRAL,
		MOVING_HORIZONTAL,
		MOVING_VERTICAL,
		PRESSING_A,
		PRESSING_L,
		PRESSING_Y,
		PRESSING_B,
		PRESSING_START,
		PROCESSING_CHARACTER,
		CLEARING_BUFFER
	} state = State::IDLE;
	
	static absolute_time_t stateStartTime = get_absolute_time();
	static Utf8Char currentChar = getEmptyChar();
	// Track last movement direction: 0 = none, 1 = horizontal, 2 = vertical
	static uint8_t lastMovementDir = 0;

	// Start with neutral state
	report = defaultGcReport;

	// If we're in town tune mode, handle that separately
	if (TownTunes::isInTownTuneMode()) {
		// Initialize arrow key d-pad state
		uint8_t arrowKeyDpad = 0;
		
		// Check for arrow key activations in devices (same logic as in handlePassthrough)
		if (device1.initialized && device1.is_keyboard) {
			// Left arrow (0x5C) or Fn+Left (0x08)
			if (tracker_is_active(&device1.key_tracker, 0x5C) || tracker_is_active(&device1.key_tracker, 0x08)) {
				arrowKeyDpad |= 0x01; // D-Left
			}
			// Right arrow (0x5F) or Fn+Right (0x07)
			if (tracker_is_active(&device1.key_tracker, 0x5F) || tracker_is_active(&device1.key_tracker, 0x07)) {
				arrowKeyDpad |= 0x02; // D-Right
			}
		}
		
		if (device2.initialized && device2.is_keyboard) {
			// Left arrow (0x5C)
			if (tracker_is_active(&device2.key_tracker, 0x5C)) {
				arrowKeyDpad |= 0x01; // D-Left
			}
			// Right arrow (0x5F)
			if (tracker_is_active(&device2.key_tracker, 0x5F)) {
				arrowKeyDpad |= 0x02; // D-Right
			}
		}
		
		// Keep track of button state changes for edge detection
		static uint8_t lastDpadState = 0;
		static uint8_t lastArrowKeyDpad = 0;
		static bool lastXButtonState = false;
		bool currentXButtonState = (buttons1 & 0x04) != 0;
		bool xJustPressed = currentXButtonState && !lastXButtonState;
		
		// Edge detection for left/right presses
		bool leftJustPressed = ((dpadState & 0x01) && !(lastDpadState & 0x01)) || 
							((arrowKeyDpad & 0x01) && !(lastArrowKeyDpad & 0x01));
		bool rightJustPressed = ((dpadState & 0x02) && !(lastDpadState & 0x02)) || 
								((arrowKeyDpad & 0x02) && !(lastArrowKeyDpad & 0x02));
		
		// Update state tracking
		lastDpadState = dpadState;
		lastArrowKeyDpad = arrowKeyDpad;
		lastXButtonState = currentXButtonState;
		
		// Process the town tune state machine
		TownTunes::processTownTune(report, xJustPressed, leftJustPressed, rightJustPressed, 
								  (buttons1 & 0x10) != 0, simulatedState.hold_duration_us);
		return;
	}
	
	// If we're in snake mode, handle that separately
	if (Snake::isInSnakeMode()) {
		// Snake::processSnake now handles its own state and reads the shared initialKeyCodeBuffer when needed.
		Snake::processSnake(report, simulatedState.hold_duration_us);
		// Check if snake is waiting for the physical Start button
		if (Snake::getCurrentState() == Snake::SnakeState::WAIT_FOR_START) {
			if (buttons1 & 0x10) { // Check physical Start button
				Snake::startGame();
			}
		}
		return; // Exit after handling snake
	}
	
	// If we're in design mode, handle that separately
	if (Design::isInDesignMode()) {
		// Check for 'S' key press to enter snake mode
		bool s_key_pressed = false;
		
		// Check both devices for 'S' key (0x22)
		if (device1.initialized && device1.is_keyboard) {
			if (tracker_is_active(&device1.key_tracker, 0x22)) {
				s_key_pressed = true;
			}
		}
		
		if (device2.initialized && device2.is_keyboard) {
			if (tracker_is_active(&device2.key_tracker, 0x22)) {
				s_key_pressed = true;
			}
		}
		
		if (s_key_pressed) {
			// Access the design cursor position before exiting design mode
			extern int design_currentX;
			extern int design_currentY;

			// Store the position before exiting design mode
			int posX = design_currentX;
			int posY = design_currentY;
	  
			// Exit design mode and enter snake mode
			Design::exitDesignMode();
			Snake::enterSnakeMode(currentPalette, currentColor, posX, posY);
			return;
		}
		
		// Process the design state machine
		Design::processDesign(report, simulatedState.hold_duration_us);
		return;
	}

	absolute_time_t currentTime = get_absolute_time();
	int64_t elapsed_us = absolute_time_diff_us(stateStartTime, currentTime);
	bool stateWillChange = elapsed_us >= simulatedState.hold_duration_us;
	bool allowPassthrough = (state == State::IDLE && keyBuffer.isEmpty()) & !NookCodes::isInNookCodeMode();

	// Handle passthrough mode
	if (allowPassthrough) {
		handlePassthrough(report, buttons1, buttons2, dpadState, analogX, analogY, cX, cY);
		return;
	}

	switch (state) {
		case State::IDLE: {
			if (needsBackspace) {
				state = State::PRESSING_B;
				stateStartTime = currentTime;
				break;
			}
			
			if (NookCodes::shouldClearBuffer()) {
				state = State::CLEARING_BUFFER;
				stateStartTime = currentTime;
				break;
			}
			
			if (isEmptyChar(currentChar) && keyBuffer.pop(currentChar)) {
				if (isKeyCharacter(currentChar)) {
					if (NookCodes::isInNookCodeMode()) {
						// Exit nook code mode
						NookCodes::exitNookCodeMode();
					} else {
						// Enter nook code mode
						NookCodes::enterNookCodeMode();
						state = State::CLEARING_BUFFER;
					}
					stateStartTime = currentTime;
					currentChar = getEmptyChar();
					break;
				}
				
				if (isFrogCharacter(currentChar)) {
					// Enter town tune mode
					TownTunes::enterTownTuneMode();
					currentChar = getEmptyChar();
					break;
				}
				
				if (isPaintCharacter(currentChar)) {
					// Enter design mode
					Design::enterDesignMode();
					currentChar = getEmptyChar();
					break;
				}
				
				if (!simulatedState.keyboard_calibrated) {
					state = State::CALIBRATING;
					KeyboardCalibration::reset();
					lastMovementDir = 0;  // Reset direction tracking
					stateStartTime = currentTime;
					break;
				}
				
				auto positions = findCharacterPositions(currentChar);
				if (!positions.empty()) {
					targetPos = findClosestPosition(positions);
					state = State::NEUTRAL;
					stateStartTime = currentTime;
				} else {
					currentChar = getEmptyChar();
				}
			}
			break;
		}

		case State::CALIBRATING: {
			auto currentMove = KeyboardCalibration::getCurrentMove();
			report.xStick = currentMove.xStick;
			report.yStick = currentMove.yStick;
			
			if (stateWillChange) {
				KeyboardCalibration::advance();
				if (KeyboardCalibration::isComplete()) {
					currentPos = {0, 0, 0};
					simulatedState.keyboard_calibrated = true;
					lastMovementDir = 0;  // Reset direction tracking after calibration
					
					if (!isEmptyChar(currentChar)) {
						auto positions = findCharacterPositions(currentChar);
						if (!positions.empty()) {
							targetPos = findClosestPosition(positions);
						}
					}
					
					state = State::NEUTRAL;
					stateStartTime = currentTime;
				} else {
					stateStartTime = currentTime;
				}
			}
			break;
		}

		case State::NEUTRAL: {
			if (stateWillChange) {
				if (NookCodes::shouldPressStart() && keyBuffer.isEmpty() && isEmptyChar(currentChar)) {
					state = State::PRESSING_START;
					stateStartTime = currentTime;
					NookCodes::clearNeedToPressStart();
				} else if (!isEmptyChar(currentChar)) {
					if (currentPos.layer != targetPos.layer) {
						if ((currentPos.layer <= 1 && targetPos.layer <= 1) ||
							(currentPos.layer == 0 && targetPos.layer == 1) ||
							(currentPos.layer == 1 && targetPos.layer == 0)) {
							state = State::PRESSING_L;
						} else {
							state = State::PRESSING_Y;
						}
					} else if (currentPos.col != targetPos.col) {
						state = State::MOVING_HORIZONTAL;
					} else if (currentPos.row != targetPos.row) {
						state = State::MOVING_VERTICAL;
					} else {
						state = State::PRESSING_A;
					}
				} else {
					state = State::IDLE;
				}
				stateStartTime = currentTime;
			}
			break;
		}

		case State::MOVING_HORIZONTAL: {
			if (currentPos.col < targetPos.col) {
				report.xStick = 255;
			} else if (currentPos.col > targetPos.col) {
				report.xStick = 0;
			}
			
			if (stateWillChange) {
				if (currentPos.col < targetPos.col) currentPos.col++;
				else if (currentPos.col > targetPos.col) currentPos.col--;
				
				// Check if we can zigzag to vertical movement
				if (currentPos.row != targetPos.row && lastMovementDir != 1) {
					state = State::MOVING_VERTICAL;
				} else if (currentPos.col != targetPos.col) {
					state = State::NEUTRAL;  // Need neutral state between same-direction moves
				} else if (currentPos.row != targetPos.row) {
					state = State::MOVING_VERTICAL;
				} else {
					state = State::PRESSING_A;
				}
				lastMovementDir = 1;
				stateStartTime = currentTime;
			}
			break;
		}

		case State::MOVING_VERTICAL: {
			if (currentPos.row < targetPos.row) {
				report.yStick = 0;
			} else if (currentPos.row > targetPos.row) {
				report.yStick = 255;
			}
			
			if (stateWillChange) {
				if (currentPos.row < targetPos.row) currentPos.row++;
				else if (currentPos.row > targetPos.row) currentPos.row--;
				
				// Check if we can zigzag to horizontal movement
				if (currentPos.col != targetPos.col && lastMovementDir != 2) {
					state = State::MOVING_HORIZONTAL;
				} else if (currentPos.row != targetPos.row) {
					state = State::NEUTRAL;  // Need neutral state between same-direction moves
				} else if (currentPos.col != targetPos.col) {
					state = State::MOVING_HORIZONTAL;
				} else {
					state = State::PRESSING_A;
				}
				lastMovementDir = 2;
				stateStartTime = currentTime;
			}
			break;
		}

		case State::PRESSING_A: {
			auto& itemName = NookCodes::getItemName();
			bool nookCodeModeActive = NookCodes::isInNookCodeMode();
			
			if (!nookCodeModeActive || (nookCodeModeActive && itemName.size() < 28)) {
				report.a = 1;
			}
		
			if (stateWillChange) {
				if (nookCodeModeActive && itemName.size() < 28) {
					// Only go to PROCESSING_CHARACTER if we're in nook code mode and need to process
					NookCodes::addCharToItemName(currentChar);
					state = State::PROCESSING_CHARACTER;
				} else {
					// Otherwise, handle normally
					currentChar = getEmptyChar(); // Reset direction tracking after completing a character
					lastMovementDir = 0;
					state = State::NEUTRAL;
				}
				stateStartTime = currentTime;
			}
			break;
		}

		case State::PROCESSING_CHARACTER: {
			// Process the nook code matching without affecting button timing
			NookCodes::checkAndProcessNookCode();
			currentChar = getEmptyChar();
			lastMovementDir = 0;
			state = State::NEUTRAL;
			stateStartTime = currentTime;
			break;
		}

		case State::PRESSING_L: {
			report.l = 1;
			report.analogL = 255;
			if (stateWillChange) {
				currentPos.layer = (currentPos.layer == 0) ? 1 : 0;
				state = State::NEUTRAL;
				stateStartTime = currentTime;
			}
			break;
		}

		case State::PRESSING_Y: {
			report.y = 1;
			if (stateWillChange) {
				if (currentPos.layer <= 1) currentPos.layer = 2;
				else if (currentPos.layer == 2) currentPos.layer = 3;
				else currentPos.layer = 0;
				state = State::NEUTRAL;
				stateStartTime = currentTime;
			}
			break;
		}

		case State::PRESSING_B: {
			report.b = 1;
			if (stateWillChange) {
				needsBackspace = false;
				state = State::NEUTRAL;
				stateStartTime = currentTime;
			}
			break;
		}

		case State::PRESSING_START: {
			report.start = 1;
			if (stateWillChange) {
				state = State::NEUTRAL;
				simulatedState.keyboard_calibrated = false;
				stateStartTime = currentTime;
			}
			break;
		}

		case State::CLEARING_BUFFER: {
			static int clearCount = 0;
			static bool pressingB = true;
			
			if (pressingB) {
				report.b = 1;
			}
			
			if (stateWillChange) {
				if (pressingB) {
					pressingB = false;
				} else {
					pressingB = true;
					clearCount++;
				}
				
				if (clearCount >= 28) {
					clearCount = 0;
					NookCodes::clearNeedToClearBuffer();
					state = State::NEUTRAL;
				}
				stateStartTime = currentTime;
			}
			break;
		}
	}
}

GCReport getControllerState() {
	GCReport report = defaultGcReport;
	
	uint8_t dpadState = 0;
	uint8_t buttons1 = 0;  // byte 0 buttons (A, B, X, Y, Start)
	uint8_t buttons2 = 0;  // byte 1 buttons (Z, L, R, etc)
	uint8_t analogX = 128;  // default to centered
	uint8_t analogY = 128;
	uint8_t cX = 128;
	uint8_t cY = 128;
	
	static bool last_backspace = false;
	
	// Handle device 1
	if (device1.initialized) {
		if (device1.is_keyboard) {
			if (NookCodes::isInNookCodeMode()) {
				// Edge detection - only trigger on press, not hold
				if (device1.backspace_held && !last_backspace) {
					if (NookCodes::processBackspace()) {
						needsBackspace = true;
					}
				}
				last_backspace = device1.backspace_held;
			} else {
				// Normal behavior - holding backspace sends held B button
				buttons1 |= device1.backspace_held ? 0x02 : 0;
			}
		} else {
			buttons1 |= device1.last_state[0];  // All buttons from byte 0
			buttons2 |= device1.last_state[1];  // All buttons from byte 1
			dpadState |= (device1.last_state[1] & 0x0F);
			analogX = device1.last_state[2];  // Analog stick X
			analogY = device1.last_state[3];  // Analog stick Y
			cX = device1.last_state[4];      // C-stick X
			cY = device1.last_state[5];      // C-stick Y
		}
	}

	// Handle device 2
	if (device2.initialized) {
		if (device2.is_keyboard) {
			if (NookCodes::isInNookCodeMode()) {
				// Edge detection - only trigger on press, not hold
				if (device2.backspace_held && !last_backspace) {
					if (NookCodes::processBackspace()) {
						needsBackspace = true;
					}
				}
				last_backspace = device2.backspace_held;
			} else {
				// Normal behavior - holding backspace sends held B button
				buttons1 |= device2.backspace_held ? 0x02 : 0;
			}
		} else {
			buttons1 |= device2.last_state[0];  // All buttons from byte 0
			buttons2 |= device2.last_state[1];  // All buttons from byte 1
			dpadState |= (device2.last_state[1] & 0x0F);
			// Only override analog values if device1 isn't providing them
			if (!device1.initialized || device1.is_keyboard) {
				analogX = device2.last_state[2];
				analogY = device2.last_state[3];
				cX = device2.last_state[4];
				cY = device2.last_state[5];
			}
		}
	}

	if (Snake::isExpectingInitials()) {
		// Scan keycodes A-Z (0x10 to 0x29)
		for (uint8_t kc = 0x10; kc <= 0x29; ++kc) {
			bool isActive = false;
			// Check both devices to see if they are keyboards
			if (device1.initialized && device1.is_keyboard && tracker_is_active(&device1.key_tracker, kc)) {
				isActive = true;
			}
			if (!isActive && device2.initialized && device2.is_keyboard && tracker_is_active(&device2.key_tracker, kc)) {
				isActive = true;
			}

			bool wasActive = initialKeyState[kc]; // Reads false if key wasn't present before

			if (isActive && !wasActive) { // Rising edge detected
				// Check buffer size conceptually (max 3 initials allowed)
				// Snake::isExpectingInitials should return false once 3 are entered/processed,
				// but this check prevents overfilling the buffer just in case.
				if (initialKeyCodeBuffer.size() < 3) {
					initialKeyCodeBuffer.push(kc); // Add to the shared buffer
				}
			}
			initialKeyState[kc] = isActive; // Update current state for next frame's edge detection
		}
	} else {
		// If not expecting initials, clear the key state tracking map
		if (!initialKeyState.empty()) {
			initialKeyState.clear();
		}
	}
	
	processKeyBuffer(report, (buttons1 & 0x02) != 0, dpadState, buttons1, buttons2, 
					analogX, analogY, cX, cY);
	
	return report;
}