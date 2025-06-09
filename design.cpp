#include "design.hpp"
#include "simulatedController.hpp"
#include "types.hpp"
#include "gcReport.hpp"
#include <cstdio>
#include <cstring>
#include <pico/stdlib.h>

bool isPaintCharacter(const Utf8Char& c) {
	// Compare with UTF-8 bytes for 🎨
	static const uint8_t paintChar[] = {0xF0, 0x9F, 0x8E, 0xA8};
	if (c.length != 4) return false;
	return memcmp(c.bytes, paintChar, 4) == 0;
}

// External declarations
extern DeviceState device1;
extern DeviceState device2;
extern KeyBuffer keyBuffer;
extern SimulatedState simulatedState;

// Design mode state variables
static bool inDesignMode = false;
static Design::DesignState designState = Design::DesignState::INIT_CALIBRATE;
static Design::Frameset currentFrameset;
static bool designSequenceStarted = false;

// Current position tracking
int design_currentX = 0;         // Made non-static to be accessible from other files
int design_currentY = 0;         // Made non-static to be accessible from other files
uint8_t currentPalette = 0;      // Made non-static to be accessible from snake.cpp
uint8_t currentColor = 1;        // Position in the color menu (0 = change palette button, 1-15 = colors; starts at 1)
static uint8_t lastColor = 1;    // Store the last selected color before palette change
static int calibrationStep = 0;   // Used for initial cursor positioning

// Target position for drawing
static int targetX = 0;
static int targetY = 0;
static uint8_t targetColor = 0;
static bool movingRight = true;   // Direction for "snaking" path, true = right, false = left

namespace Design {

	bool isInDesignMode() {
		return inDesignMode;
	}

	void enterDesignMode() {
		inDesignMode = true;
		designSequenceStarted = false;
		designState = DesignState::INIT_CALIBRATE;
		calibrationStep = 0;
		design_currentX = 0;
		design_currentY = 0;
		currentPalette = 0;
		currentColor = 1;  // Start at position 1 (first color in the palette menu)
		lastColor = 1;     // Initialize last color
		movingRight = true; // Start with right motion in snaking pattern
		targetX = 0;
		targetY = 0;
		targetColor = 0;
		if (currentFrameset.frames.empty()) {
			initFrameset();
		}
		currentFrameset.currentFrameIndex = 0; // Start with the first frame
		keyBuffer.clear();
	}

	void processDesign(GCReport& report, uint64_t hold_duration_us) {
		static absolute_time_t stateStartTime = get_absolute_time();
		absolute_time_t currentTime = get_absolute_time();
		int64_t elapsed_us = absolute_time_diff_us(stateStartTime, currentTime);
		
		// Start with neutral controller state by default
		report = defaultGcReport;
		
		// Check for state change based on timing
		bool stateWillChange = elapsed_us >= hold_duration_us;
		
		// Initialize state machine when first entering design mode
		if (!designSequenceStarted) {
			designSequenceStarted = true;
			stateStartTime = currentTime;
			return;
		}
		
		// Simplified state machine with explicit neutral states
		switch (designState) {
			case DesignState::INIT_CALIBRATE:
				// Move cursor diagonally up-left
				report.xStick = 0;   // Full left
				report.yStick = 255; // Full up
				
				if (stateWillChange) {
					calibrationStep++;
					designState = DesignState::CALIBRATE_NEUTRAL;
					stateStartTime = currentTime;
				}
				break;
				
			case DesignState::CALIBRATE_NEUTRAL:
				// Neutral state (already set at the beginning)
				
				if (stateWillChange) {
					if (calibrationStep >= 31) {
						// Calibration complete
						design_currentX = 0;
						design_currentY = 0;
						calibrationStep = 0;
						
						// Transition to settling state to allow frame loading to complete
						designState = DesignState::FRAME_LOADING_SETTLING;
					} else {
						// Continue calibration
						designState = DesignState::INIT_CALIBRATE;
					}
					stateStartTime = currentTime;
				}
				break;
				
			case DesignState::MOVE_TO_PALETTE_MENU:
				// Press R to enter palette menu
				report.r = 1;
				report.analogR = 255;
				
				if (stateWillChange) {
					designState = DesignState::PALETTE_MENU_NEUTRAL;
					stateStartTime = currentTime;
				}
				break;
				
			case DesignState::PALETTE_MENU_NEUTRAL:
				// Neutral state (already set at the beginning)
				
				if (stateWillChange) {
					// We are now in the palette menu, next we need to navigate to position 0
					designState = DesignState::PALETTE_MENU_NAVIGATION;
					stateStartTime = currentTime;
				}
				break;
				
			case DesignState::PALETTE_MENU_NAVIGATION:
				{
					// Calculate shortest path to position 0 (palette change button)
					int distanceUp = currentColor; // Moving up to reach 0
					int distanceDown = 16 - currentColor; // Moving down (wrapping) to reach 0
					
					if (distanceUp <= distanceDown) {
						// Move up
						report.yStick = 255; // Full up
					} else {
						// Move down
						report.yStick = 0; // Full down
					}
					
					if (stateWillChange) {
						// Update cursor position based on movement direction
						if (distanceUp <= distanceDown) {
							// Moving up (with wrap)
							currentColor = (currentColor - 1 + 16) % 16;
						} else {
							// Moving down (with wrap)
							currentColor = (currentColor + 1) % 16;
						}
						
						designState = DesignState::PALETTE_NAV_NEUTRAL;
						stateStartTime = currentTime;
					}
				}
				break;
				
			case DesignState::PALETTE_NAV_NEUTRAL:
				// Neutral state (already set at the beginning)
				
				if (stateWillChange) {
					if (currentColor == 0) {
						// We've reached the palette change button, press it
						designState = DesignState::CHANGE_PALETTE_BUTTON;
					} else {
						// Continue navigation
						designState = DesignState::PALETTE_MENU_NAVIGATION;
					}
					stateStartTime = currentTime;
				}
				break;
				
			case DesignState::CHANGE_PALETTE_BUTTON:
				// Press A to change palette
				report.a = 1;
				
				if (stateWillChange) {
					designState = DesignState::PALETTE_BUTTON_NEUTRAL;
					stateStartTime = currentTime;
				}
				break;
				
			case DesignState::PALETTE_BUTTON_NEUTRAL:
				// Neutral state (already set at the beginning)
				
				if (stateWillChange) {
					// Increment the palette (with wrap)
					currentPalette = (currentPalette + 1) % 16;
					uint8_t targetPaletteId = getCurrentPaletteId();
					
					if (currentPalette == targetPaletteId) {
						// We've reached the target palette, return to canvas
						designState = DesignState::RETURN_TO_CANVAS;
					} else {
						// Need to press A again
						designState = DesignState::CHANGE_PALETTE_BUTTON;
					}
					stateStartTime = currentTime;
				}
				break;
				
			case DesignState::RETURN_TO_CANVAS:
				// Press L to return to canvas
				report.l = 1;
				report.analogL = 255;
				
				if (stateWillChange) {
					designState = DesignState::RETURN_CANVAS_NEUTRAL;
					stateStartTime = currentTime;
				}
				break;
				
			case DesignState::RETURN_CANVAS_NEUTRAL:
				// Neutral state (already set at the beginning)
				
				if (stateWillChange) {
					// We've returned to the canvas
					// Reset current color to the lastColor that was active before palette change
					currentColor = lastColor;
					
					// Start drawing pixels
					// Initialize the first pixel to draw
					targetX = 0;
					targetY = 0;
					const FrameData& currentFrame = getCurrentFrame();
					targetColor = currentFrame.pixels[targetY][targetX] + 1; // +1 because UI has indices 1-15
					designState = DesignState::SELECT_COLOR;
					stateStartTime = currentTime;
				}
				break;
				
			case DesignState::SELECT_COLOR:
				{
					// Only do color selection if we're not already on the right color
					if (currentColor != targetColor) {
						// Calculate shortest path to target color
						// Remember: position 0 is the palette change button, which we want to avoid
						// and we need to wrap from position 1 to position 15
						
						// First, determine if we need to go up or down
						int distanceUp, distanceDown;
						
						// Special case: we're at color 0 (palette change)
						if (currentColor == 0) {
							// Always move down to get to color 1 first
							report.cyStick = 0; // C-stick down
							if (stateWillChange) {
								currentColor = 1;
								designState = DesignState::SELECT_COLOR_NEUTRAL;
								stateStartTime = currentTime;
							}
							break;
						}
						
						// Calculate shortest distance with wrapping, avoiding position 0
						if (targetColor > currentColor) {
							distanceUp = 15 - targetColor + currentColor; // Going up and wrapping
							distanceDown = targetColor - currentColor;    // Going down directly
						} else { // targetColor < currentColor
							distanceUp = currentColor - targetColor;      // Going up directly
							distanceDown = 15 - currentColor + targetColor; // Going down and wrapping
						}
						
						// Move in the direction of the shortest path
						if (distanceUp < distanceDown) {
							// Move up with C-stick
							report.cyStick = 255; // C-stick up
						} else {
							// Move down with C-stick
							report.cyStick = 0;   // C-stick down
						}
						
						if (stateWillChange) {
							// Update the current color position based on movement
							if (distanceUp < distanceDown) {
								// Moving up (with wrap from 1 to 15)
								if (currentColor == 1) {
									currentColor = 15;
								} else {
									currentColor--;
								}
							} else {
								// Moving down (with wrap from 15 to 1)
								if (currentColor == 15) {
									currentColor = 1;
								} else {
									currentColor++;
								}
							}
							designState = DesignState::SELECT_COLOR_NEUTRAL;
							stateStartTime = currentTime;
						}
					} else {
						// Already on the right color, draw the pixel
						designState = DesignState::DRAW_PIXEL;
						stateStartTime = currentTime;
					}
				}
				break;
				
			case DesignState::SELECT_COLOR_NEUTRAL:
				// Neutral state after c-stick movement
				
				if (stateWillChange) {
					if (currentColor == targetColor) {
						// We've reached the target color, draw the pixel
						designState = DesignState::DRAW_PIXEL;
					} else {
						// Continue color selection
						designState = DesignState::SELECT_COLOR;
					}
					stateStartTime = currentTime;
				}
				break;
				
			case DesignState::DRAW_PIXEL:
				// Press A to draw the pixel - needs longer duration to be recognized
				report.a = 1;
				
				if (elapsed_us >= hold_duration_us * 2) { // Hold for 34000μs (twice the standard hold duration)
					designState = DesignState::DRAW_PIXEL_NEUTRAL;
					stateStartTime = currentTime;
				}
				break;
				
			case DesignState::DRAW_PIXEL_NEUTRAL:
				// Neutral state after drawing a pixel - using standard duration
				
				if (stateWillChange) {
					// Check if we've completed the current row/column
					bool isDone = false;
					bool needsVerticalMove = false;
					
					// Get the current frame
					const FrameData& currentFrame = getCurrentFrame();
					
					// Calculate next position in snaking pattern
					if (movingRight) {
						// Moving right
						if (targetX < 31) {
							targetX++;
						} else {
							// End of row, move down and change direction
							if (targetY < 31) {
								targetY++;
								movingRight = false;
								needsVerticalMove = true;
							} else {
								// End of frame
								isDone = true;
							}
						}
					} else {
						// Moving left
						if (targetX > 0) {
							targetX--;
						} else {
							// End of row, move down and change direction
							if (targetY < 31) {
								targetY++;
								movingRight = true;
								needsVerticalMove = true;
							} else {
								// End of frame
								isDone = true;
							}
						}
					}
					
					if (isDone) {
						// Check if there's another frame to process
						if (currentFrameset.currentFrameIndex + 1 < getFrameCount()) {
							designState = DesignState::NEXT_FRAME;
						} else {
							// Frameset complete, exit design mode
							designState = DesignState::EXIT_DESIGN;
						}
					} else {
						// Get next target color
						targetColor = currentFrame.pixels[targetY][targetX] + 1; // +1 because UI has indices 1-15
						
						// Determine if we need to move vertically or horizontally
						if (needsVerticalMove) {
							// Move down one row
							designState = DesignState::MOVE_CURSOR;
						} else {
							// Continue in the same row
							designState = DesignState::MOVE_CURSOR;
						}
					}
					stateStartTime = currentTime;
				}
				break;
				
			case DesignState::MOVE_CURSOR:
				// Move the cursor based on whether we need to move horizontally or vertically
				if (targetY > design_currentY) {
					// Need to move down
					report.yStick = 0; // Down
					if (stateWillChange) {
						design_currentY++; // Update the current position
					}
				} else if (movingRight && targetX > design_currentX) {
					// Moving right
					report.xStick = 255; // Right
					if (stateWillChange) {
						design_currentX++; // Update the current position
					}
				} else if (!movingRight && targetX < design_currentX) {
					// Moving left
					report.xStick = 0; // Left
					if (stateWillChange) {
						design_currentX--; // Update the current position
					}
				}
				
				if (stateWillChange) {
					designState = DesignState::MOVE_CURSOR_NEUTRAL;
					stateStartTime = currentTime;
				}
				break;
				
			case DesignState::MOVE_CURSOR_NEUTRAL:
				// Neutral state after moving the cursor
				
				if (stateWillChange) {
					// Now select the color for the next pixel
					designState = DesignState::SELECT_COLOR;
					stateStartTime = currentTime;
				}
				break;
				
			case DesignState::NEXT_FRAME:
				// Move to the next frame
				if (stateWillChange) {
					currentFrameset.currentFrameIndex++;
					
					// Reset for the next frame
					targetX = 0;
					targetY = 0;
					movingRight = true;
					
					// Reset calibration
					designState = DesignState::INIT_CALIBRATE;
					calibrationStep = 0;
					stateStartTime = currentTime;
				}
				break;
				
			case DesignState::FRAME_LOADING_SETTLING:
				{
					// Neutral state that waits for several standard durations to allow frame loading to settle
					// This prevents A button presses and palette switching from being disturbed by frame loading
					static bool frameSetupDone = false;
					static uint8_t targetPaletteId = 0;
					
					// Only do frame/palette setup once when entering this state
					if (!frameSetupDone) {
						targetX = 0;
						targetY = 0;
						const FrameData& currentFrame = getCurrentFrame();
						targetColor = currentFrame.pixels[targetY][targetX] + 1; // +1 because UI has indices 1-15
						
						// Check if we need to change palette
						targetPaletteId = getCurrentPaletteId();
						frameSetupDone = true;
					}
					
					if (elapsed_us >= hold_duration_us * 20) { // Wait for 20 standard durations
						// Frame loading has settled, now check palette and start drawing
						frameSetupDone = false; // Reset for next time this state is entered
						
						if (currentPalette != targetPaletteId) {
							// Store the current color before entering palette menu
							lastColor = currentColor;
							designState = DesignState::MOVE_TO_PALETTE_MENU;
						} else {
							// Start drawing pixels
							designState = DesignState::SELECT_COLOR;
						}
						stateStartTime = currentTime;
					}
				}
				break;
				
			case DesignState::EXIT_DESIGN:
				// Press Start to exit design mode
				report.start = 1;
				
				if (stateWillChange) {
					designState = DesignState::EXIT_NEUTRAL;
					stateStartTime = currentTime;
				}
				break;
				
			case DesignState::EXIT_NEUTRAL:
				// Neutral state after pressing Start
				
				if (stateWillChange) {
					// Exit design mode
					inDesignMode = false;
					stateStartTime = currentTime;
				}
				break;
				
			case DesignState::WAITING:
				// Just wait in neutral state until user action
				break;
		}
	}
	
	void exitDesignMode() {
		inDesignMode = false;
		designState = DesignState::INIT_CALIBRATE;
		designSequenceStarted = false;
		design_currentX = 0;
		design_currentY = 0;
		targetX = 0;
		targetY = 0;
		movingRight = true;
		calibrationStep = 0;
	}
	
	void initDefaultFrameset() {
		// Clear any existing frames
		currentFrameset.frames.clear();
		
		// Create a new frame with checkerboard pattern
		FrameData checkerboardFrame;
		checkerboardFrame.paletteId = 1;  // Use palette 1
		
		// Create a checkerboard pattern with colors 13 and 14
		for (int y = 0; y < 32; y++) {
			for (int x = 0; x < 32; x++) {
				// Alternating pattern - use 13 and 14 as colors
				checkerboardFrame.pixels[y][x] = ((x + y) % 2 == 0) ? 13 : 14;
			}
		}
		
		// Add frame to the frameset
		currentFrameset.frames.push_back(checkerboardFrame);
		currentFrameset.currentFrameIndex = 0;
	}

	Frameset& getCurrentFrameset() {
		return currentFrameset;
	}
	
	size_t getFrameCount() {
		if (currentFrameset.provider) {
			return currentFrameset.provider->getFrameCount();
		}
		return currentFrameset.frames.size();
	}
	
	const FrameData& getCurrentFrame() {
		if (currentFrameset.provider) {
			return currentFrameset.provider->getFrame(currentFrameset.currentFrameIndex);
		}
		return currentFrameset.frames[currentFrameset.currentFrameIndex];
	}
	
	uint8_t getCurrentPaletteId() {
		if (currentFrameset.provider) {
			return currentFrameset.provider->getPaletteId(currentFrameset.currentFrameIndex);
		}
		return currentFrameset.frames[currentFrameset.currentFrameIndex].paletteId;
	}
	
	// StreamingFrameProvider implementation
	uint8_t StreamingFrameProvider::getPaletteId(size_t index) const {
		if (index >= frameCount) return 0;
		
		// Frame data format: [paletteId][1024 bytes of pixel data]
		const uint8_t* frameStart = frameData + index * (1 + 32 * 32);
		return frameStart[0];
	}
	
	const FrameData& StreamingFrameProvider::getFrame(size_t index) {
		if (index >= frameCount) {
			// Return empty frame for out of bounds
			static FrameData emptyFrame = {};
			return emptyFrame;
		}
		
		// Check if we already have this frame cached
		if (cachedFrameIndex == index) {
			return currentFrame;
		}
		
		// Load frame from flash storage
		const uint8_t* frameStart = frameData + index * (1 + 32 * 32);
		currentFrame.paletteId = frameStart[0];
		
		// Copy pixel data (32x32 = 1024 bytes)
		std::memcpy(currentFrame.pixels, frameStart + 1, 32 * 32);
		
		cachedFrameIndex = index;
		return currentFrame;
	}
	
	// Initialize frameset
	void initFrameset() {
		// By default, use the generated frameset
		// To use the checkerboard pattern for debugging, uncomment the next line
		// initDefaultFrameset(); return;
		
		initGeneratedFrameset();
	}
}