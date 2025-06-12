#include "snake.hpp"
#include "simulatedController.hpp"
#include "types.hpp"
#include "gcReport.hpp"
#include <cstdlib> // For abs(), random functions
#include <pico/stdlib.h>
#include <queue>
#include <vector>
#include <utility>
#include <algorithm> // For std::max, std::min, std::abs
#include <cstdio>
#include <deque>
#include <string> // For std::to_string
#include <cstring> // For strncpy, strnlen

// External declarations
extern DeviceState device1;
extern DeviceState device2;
extern KeyBuffer keyBuffer; // Main typing buffer
extern SimulatedState simulatedState;
extern bool tracker_is_active(KeyTracker* tracker, uint8_t keycode);
extern std::queue<uint8_t> initialKeyCodeBuffer; // Use the shared buffer

// Snake mode state variables
static bool inSnakeMode = false;
static Snake::SnakeState snakeState;
static bool titleScreenDrawn = false;
static bool gameOverSequenceStarted = false;

// Current position tracking (State variables updated by helpers)
static int currentX;
static int currentY;
static uint8_t currentPalette;
static uint8_t currentColor; // Position in the color menu (0 = button, 1-15 = colors)

// Queue for cursor movement commands
static std::queue<Snake::SnakeState> movementQueue;

// Static variable for state timer
static absolute_time_t stateStartTime = 0;

// Snake game state
static std::deque<Snake::Position> snakeSegments;
static Snake::Position apple;
static Snake::Direction currentDirection = Snake::Direction::RIGHT;
static Snake::Direction nextDirection = Snake::Direction::RIGHT;
static bool gameActive = false;
static int snakeHighScore = 0;
static char snakeHighScoreInitials[4] = "HAI";
static int initialsEnteredCount = 0;
static bool newHighScore = false;
static char enteredInitials[4] = {'\0'}; // Buffer for entered initials + null terminator

// Game board (32x32 grid)
static Snake::TileType gameBoard[32][32];

namespace {
	// Helper to get the drawing vector for a character based on keycode
	const std::vector<std::pair<int, int>>* getCharVectorForKeycode(uint8_t keycode) {
		if (keycode < 0x10 || keycode > 0x29) return nullptr; // Not A-Z
		char c = 'A' + (keycode - 0x10);
		switch (c) {
			case 'A': return &Snake::charA; case 'B': return &Snake::charB; case 'C': return &Snake::charC;
			case 'D': return &Snake::charD; case 'E': return &Snake::charE; case 'F': return &Snake::charF;
			case 'G': return &Snake::charG; case 'H': return &Snake::charH; case 'I': return &Snake::charI;
			case 'J': return &Snake::charJ; case 'K': return &Snake::charK; case 'L': return &Snake::charL;
			case 'M': return &Snake::charM; case 'N': return &Snake::charN; case 'O': return &Snake::charO;
			case 'P': return &Snake::charP; case 'Q': return &Snake::charQ; case 'R': return &Snake::charR;
			case 'S': return &Snake::charS; case 'T': return &Snake::charT; case 'U': return &Snake::charU;
			case 'V': return &Snake::charV; case 'W': return &Snake::charW; case 'X': return &Snake::charX;
			case 'Y': return &Snake::charY; case 'Z': return &Snake::charZ;
			default: return nullptr;
		}
	}

	// Helper to get the pixel coordinates for the Nth underscore placeholder
	const std::vector<std::pair<int, int>>* getUnderscoreVector(int index) {
		static const std::vector<std::pair<int, int>> underscore1 = {{11,19},{12,19},{13,19}};
		static const std::vector<std::pair<int, int>> underscore2 = {{15,19},{16,19},{17,19}};
		static const std::vector<std::pair<int, int>> underscore3 = {{19,19},{20,19},{21,19}};
		switch(index) {
			case 0: return &underscore1; case 1: return &underscore2; case 2: return &underscore3;
			default: return nullptr;
		}
	}

	// Helper to get the top-left offset for drawing the Nth character
	std::pair<int, int> getCharOffset(int index) {
		switch(index) {
			case 0: return {11, 15}; // Offset for 1st initial
			case 1: return {15, 15}; // Offset for 2nd initial
			case 2: return {19, 15}; // Offset for 3rd initial
			default: return {0, 0}; // Invalid
		}
	}

}

namespace Snake {
	bool isInSnakeMode() {
		return inSnakeMode;
	}

	SnakeState getCurrentState() {
		return snakeState;
	}

	bool isExpectingInitials() {
		// True if game is over, the game over sequence has started,
		// we just got a new high score, and we haven't finished processing 3 initials yet.
		return inSnakeMode && !gameActive && gameOverSequenceStarted && (initialsEnteredCount < 3) && newHighScore;
	}


	void selectPalette(uint8_t targetPalette) {
		targetPalette %= 16;
		if (currentPalette == targetPalette) return;

		// Store the color that is currently selected before we enter the menu.
		// The game will restore the cursor to this color upon exiting the menu.
		uint8_t colorBeforePaletteMenu = currentColor;

		// 1. Open palette menu
		movementQueue.push(SnakeState::PRESS_R_BUTTON);

		// 2. Navigate to palette change button (pos 0) using analog stick
		//    This calculation uses the state *before* entering the menu.
		if (colorBeforePaletteMenu != 0) {
			int stepsUp = colorBeforePaletteMenu;
			int stepsDown = (16 - colorBeforePaletteMenu);
			if (stepsUp <= stepsDown) {
				for (int i = 0; i < stepsUp; ++i) movementQueue.push(SnakeState::MOVE_CURSOR_UP);
			} else {
				for (int i = 0; i < stepsDown; ++i) movementQueue.push(SnakeState::MOVE_CURSOR_DOWN);
			}
		}
		int numPresses = (targetPalette - currentPalette + 16) % 16;
		for (int i = 0; i < numPresses; ++i) movementQueue.push(SnakeState::PRESS_A_BUTTON);
		currentPalette = targetPalette;
		movementQueue.push(SnakeState::PRESS_L_BUTTON);
		currentColor = colorBeforePaletteMenu;
	}

	void selectColor(uint8_t colorId) {
		if (colorId == 0 || colorId > 15) return; // Check for invalid color ID
		if (currentColor == colorId) return; // Target color already selected

		// Cycle normally using 1-15 logic until target is reached
		while (currentColor != colorId) {
			int distanceUp, distanceDown;
			if (colorId > currentColor) {
				distanceDown = colorId - currentColor;
				distanceUp = currentColor + (15 - colorId); // Wrap up
			} else { // colorId < currentColor
				distanceUp = currentColor - colorId;
				distanceDown = colorId + (15 - currentColor);
			}
			if (distanceUp < distanceDown) {
				movementQueue.push(SnakeState::NEUTRAL);
				movementQueue.push(SnakeState::C_STICK_UP);
				currentColor = (currentColor == 1) ? 15 : currentColor - 1;
			} else {
				movementQueue.push(SnakeState::NEUTRAL);
				movementQueue.push(SnakeState::C_STICK_DOWN);
				currentColor = (currentColor == 15) ? 1 : currentColor + 1;
			}
		}
	}

	void navigateToPosition(int x, int y) {
		x = std::max(0, std::min(31, x)); y = std::max(0, std::min(31, y));
		int dx = x - currentX; int dy = y - currentY;
		if (dx == 0 && dy == 0) return;
		int absDx = std::abs(dx); int absDy = std::abs(dy);
		int diagonalSteps = std::min(absDx, absDy);
		int remainingHorizontal = absDx - diagonalSteps;
		int remainingVertical = absDy - diagonalSteps;
		bool moveRight = dx > 0; bool moveUp = dy < 0;
		for (int i = 0; i < diagonalSteps; i++) {
			if (moveRight && moveUp) movementQueue.push(SnakeState::MOVE_CURSOR_UP_RIGHT);
			else if (moveRight && !moveUp) movementQueue.push(SnakeState::MOVE_CURSOR_DOWN_RIGHT);
			else if (!moveRight && moveUp) movementQueue.push(SnakeState::MOVE_CURSOR_UP_LEFT);
			else movementQueue.push(SnakeState::MOVE_CURSOR_DOWN_LEFT);
		}
		for (int i = 0; i < remainingHorizontal; i++) {
			if (moveRight) movementQueue.push(SnakeState::MOVE_CURSOR_RIGHT);
			else movementQueue.push(SnakeState::MOVE_CURSOR_LEFT);
		}
		for (int i = 0; i < remainingVertical; i++) {
			if (moveUp) movementQueue.push(SnakeState::MOVE_CURSOR_UP);
			else movementQueue.push(SnakeState::MOVE_CURSOR_DOWN);
		}
		currentX = x; currentY = y;
	}

	void drawPixels(const std::vector<std::pair<int, int>>& coordinates, uint8_t colorId) {
		if (coordinates.empty()) return;
		selectColor(colorId);
		for (const auto& coord : coordinates) {
			navigateToPosition(coord.first, coord.second); // Navigate (updates sim state)
			movementQueue.push(SnakeState::PRESS_A_BUTTON); // Queue 'A' press
		}
	}

	void drawPixelsAtOffset(const std::vector<std::pair<int, int>>& coordinates, int offsetX, int offsetY, uint8_t colorId) {
		if (coordinates.empty()) return;
		// Create a new vector with coordinates transformed by the offset
		std::vector<std::pair<int, int>> offsetCoordinates;
		for (const auto& coord : coordinates) {
			offsetCoordinates.push_back({coord.first + offsetX, coord.second + offsetY});
		}
		// Call the original drawPixels with the transformed coordinates
		drawPixels(offsetCoordinates, colorId);
	}

	void drawPixel(int x, int y, uint8_t colorId) {
		selectColor(colorId);
		navigateToPosition(x, y);
		movementQueue.push(SnakeState::PRESS_A_BUTTON);
	}

	void drawString(int x, int y, const char* str, uint8_t colorId) {
		if (!str) return;
		int currentX = x;
		for (const char* c = str; *c != '\0'; c++) {
			const std::vector<std::pair<int, int>>* charVector = nullptr;
			
			// Match the character to its corresponding vector
			if (*c >= 'A' && *c <= 'Z') {
				switch (*c) { /* Cases A-Z */
					case 'A': charVector = &charA; break; case 'B': charVector = &charB; break; case 'C': charVector = &charC; break;
					case 'D': charVector = &charD; break; case 'E': charVector = &charE; break; case 'F': charVector = &charF; break;
					case 'G': charVector = &charG; break; case 'H': charVector = &charH; break; case 'I': charVector = &charI; break;
					case 'J': charVector = &charJ; break; case 'K': charVector = &charK; break; case 'L': charVector = &charL; break;
					case 'M': charVector = &charM; break; case 'N': charVector = &charN; break; case 'O': charVector = &charO; break;
					case 'P': charVector = &charP; break; case 'Q': charVector = &charQ; break; case 'R': charVector = &charR; break;
					case 'S': charVector = &charS; break; case 'T': charVector = &charT; break; case 'U': charVector = &charU; break;
					case 'V': charVector = &charV; break; case 'W': charVector = &charW; break; case 'X': charVector = &charX; break;
					case 'Y': charVector = &charY; break; case 'Z': charVector = &charZ; break;
				}
			} else if (*c >= '0' && *c <= '9') {
				switch (*c) { /* Cases 0-9 */
					case '0': charVector = &digit0; break; case '1': charVector = &digit1; break; case '2': charVector = &digit2; break;
					case '3': charVector = &digit3; break; case '4': charVector = &digit4; break; case '5': charVector = &digit5; break;
					case '6': charVector = &digit6; break; case '7': charVector = &digit7; break; case '8': charVector = &digit8; break;
					case '9': charVector = &digit9; break;
				}
			}
			if (charVector) { drawPixelsAtOffset(*charVector, currentX, y, colorId); }
			currentX += 4;
		}
	}

	void blanketFillWithColor(uint8_t colorId) {
		selectColor(colorId);
		movementQueue.push(SnakeState::PRESS_L_BUTTON);
		movementQueue.push(SnakeState::MOVE_CURSOR_DOWN);
		for (int i = 0; i < 5; ++i) movementQueue.push(SnakeState::MOVE_CURSOR_RIGHT);
		movementQueue.push(SnakeState::PRESS_A_BUTTON);
		movementQueue.push(SnakeState::PRESS_A_BUTTON);
		movementQueue.push(SnakeState::PRESS_L_BUTTON);
		for (int i = 0; i < 5; ++i) movementQueue.push(SnakeState::MOVE_CURSOR_LEFT);
		movementQueue.push(SnakeState::MOVE_CURSOR_UP);
		movementQueue.push(SnakeState::PRESS_A_BUTTON);
	}

	void initSnake() {
		while (!movementQueue.empty()) { movementQueue.pop(); }
		selectPalette(6);
		blanketFillWithColor(14);
		drawPixels({{7,4},{6,4},{5,5},{5,6},{6,6},{7,6},{7,7},{6,8},{5,8}}, 1); //S
		drawPixels({{9,8},{9,7},{9,6},{9,5},{9,4},{10,4},{11,4},{12,4},{12,5},{12,6},{12,7},{12,8}}, 2); //N
		drawPixels({{14,8},{14,7},{14,6},{14,5},{15,4},{16,4},{17,5},{17,6},{17,7},{17,8},{15,7},{16,7}}, 1); //A
		drawPixels({{19,4},{19,5},{19,6},{19,7},{19,8},{22,4},{21,5},{20,6},{21,7},{22,8}}, 1); //K
		drawPixels({{24,4},{24,5},{24,6},{24,7},{24,8},{25,8},{26,8},{25,6},{26,6},{25,4},{26,4}}, 1); //E
		drawPixels({{5,19},{5,20},{5,21},{5,22},{5,23},{5,24},{6,19},{7,19},{8,20},{8,21},{7,22},{6,22}}, 1); //p
		drawPixels({{10,19},{10,20},{10,21},{10,22},{11,20},{12,19}}, 1); //r
		drawPixels({{16,20},{17,20},{16,19},{15,19},{14,20},{14,21},{15,21},{15,22},{16,22}},1); //e
		drawPixels({{21,19},{20,19},{19,20},{20,20},{21,21},{20,22},{19,22}},1); //s
		drawPixels({{25,19},{24,19},{23,20},{24,20},{25,21},{24,22},{23,22}}, 1); //s
		drawPixels({{7,27},{6,27},{5,28},{6,28},{7,29},{6,30},{5,30}},1); //s
		drawPixels({{10,26},{10,27},{10,28},{10,29},{11,30},{9,27},{11,27}},1); //t
		drawPixels({{15,27},{14,27},{13,28},{13,29},{14,30},{15,30},{16,30},{16,29},{16,28}},1); //a
		drawPixels({{18,27},{18,28},{18,29},{18,30},{19,28},{20,27}},1); //r
		drawPixels({{23,26},{23,27},{23,28},{23,29},{24,30},{22,27},{24,27}},1); //t
		drawPixels({{26,26},{26,27},{26,28}},1); //! (upper)
		drawPixel(26,30,9); //! (.)
		navigateToPosition(28,30);
	}

	// Initialize game board and reset to empty state
	void initializeGameBoard() {
		// Set all tiles to empty
		for (int y = 0; y < 32; y++) {
			for (int x = 0; x < 32; x++) {
				gameBoard[y][x] = TileType::EMPTY;
			}
		}
	}

	// Initialize snake game
	void initializeGameState() {
		// Initialize game board
		initializeGameBoard();
		
		// Initial snake position - head at (12,8) and body towards left, then down, around and up
		snakeSegments.clear();
		snakeSegments = { {12, 8},{12, 7},{12, 6},{12, 5},{12, 4},{11, 4}, {10, 4},{9, 4},{9, 5},{9, 6},{9, 7},{9, 8} };
		for (const auto& segment : snakeSegments) { gameBoard[segment.y][segment.x] = TileType::SNAKE_BODY; }
		apple = {26, 30};
		gameBoard[apple.y][apple.x] = TileType::APPLE;
		currentDirection = Direction::DOWN;
		nextDirection = Direction::DOWN;
		gameActive = true;
	}

	bool checkCollision(const Position& position) {
		if (position.x < 0 || position.x >= 32 || position.y < 0 || position.y >= 32) { return true; }
		return gameBoard[position.y][position.x] == TileType::SNAKE_BODY;
	}

	bool checkAppleCollision(const Position& position) {
		return gameBoard[position.y][position.x] == TileType::APPLE;
	}

	void placeApple() {
		std::vector<Position> emptyTiles;
		for (int y = 0; y < 32; y++) {
			for (int x = 0; x < 32; x++) {
				if (gameBoard[y][x] == TileType::EMPTY) { emptyTiles.push_back({x, y}); }
			}
		}
		if (emptyTiles.empty()) { gameActive = false; return; }
		int index = rand() % emptyTiles.size();
		apple = emptyTiles[index];
		gameBoard[apple.y][apple.x] = TileType::APPLE;
		drawPixel(apple.x, apple.y, 9);
	}

	void moveSnake(Direction dir) {
		if (!gameActive) return;
		Position currentHead = snakeSegments.front();
		Position newHead = currentHead;
		switch (dir) {
			case Direction::UP: newHead.y--; break;
			case Direction::DOWN: newHead.y++; break;
			case Direction::LEFT: newHead.x--; break;
			case Direction::RIGHT: newHead.x++; break;
		}
		if (checkCollision(newHead)) { gameActive = false; return; }
		bool ateApple = checkAppleCollision(newHead);
		snakeSegments.push_front(newHead);
		gameBoard[newHead.y][newHead.x] = TileType::SNAKE_BODY;
		drawPixel(newHead.x, newHead.y, 2);
		if (ateApple) {
			placeApple();
		} else {
			Position oldTail = snakeSegments.back();
			snakeSegments.pop_back();
			gameBoard[oldTail.y][oldTail.x] = TileType::EMPTY;
			drawPixel(oldTail.x, oldTail.y, 14);
		}
	}

	bool checkDirectionalInput(DeviceState* device, int direction) {
		if (!device->initialized) return false;
		if (!device->is_keyboard) {
			switch (direction) { /* Controller D-pad */
				case 0: return (device->last_state[1] & 0x08) > 0; case 1: return (device->last_state[1] & 0x04) > 0;
				case 2: return (device->last_state[1] & 0x01) > 0; case 3: return (device->last_state[1] & 0x02) > 0;
				default: return false;
			}
		} else {
			switch (direction) { /* Keyboard Arrows */
				case 0: return tracker_is_active(&device->key_tracker, 0x5E) || tracker_is_active(&device->key_tracker, 0x06);
				case 1: return tracker_is_active(&device->key_tracker, 0x5D) || tracker_is_active(&device->key_tracker, 0x09);
				case 2: return tracker_is_active(&device->key_tracker, 0x5C) || tracker_is_active(&device->key_tracker, 0x08);
				case 3: return tracker_is_active(&device->key_tracker, 0x5F) || tracker_is_active(&device->key_tracker, 0x07);
				default: return false;
			}
		}
	}

	void updateSnakeDirection() {
		if (!inSnakeMode || !gameActive) return;
		bool dpadUp = checkDirectionalInput(&device1, 0) || checkDirectionalInput(&device2, 0);
		bool dpadDown = checkDirectionalInput(&device1, 1) || checkDirectionalInput(&device2, 1);
		bool dpadLeft = checkDirectionalInput(&device1, 2) || checkDirectionalInput(&device2, 2);
		bool dpadRight = checkDirectionalInput(&device1, 3) || checkDirectionalInput(&device2, 3);
		if (dpadUp && currentDirection != Direction::DOWN) { nextDirection = Direction::UP; }
		else if (dpadDown && currentDirection != Direction::UP) { nextDirection = Direction::DOWN; }
		else if (dpadLeft && currentDirection != Direction::RIGHT) { nextDirection = Direction::LEFT; }
		else if (dpadRight && currentDirection != Direction::LEFT) { nextDirection = Direction::RIGHT; }
	}

	void startGame() {
		if (snakeState != SnakeState::WAIT_FOR_START) { return; }
		while (!movementQueue.empty()) { movementQueue.pop(); }
		selectColor(1);
		movementQueue.push(SnakeState::PRESS_L_BUTTON); movementQueue.push(SnakeState::MOVE_CURSOR_DOWN);
		movementQueue.push(SnakeState::PRESS_A_BUTTON); movementQueue.push(SnakeState::PRESS_A_BUTTON);
		selectColor(14);
		movementQueue.push(SnakeState::PRESS_A_BUTTON); movementQueue.push(SnakeState::PRESS_A_BUTTON);
		movementQueue.push(SnakeState::PRESS_L_BUTTON); movementQueue.push(SnakeState::MOVE_CURSOR_UP);
		movementQueue.push(SnakeState::PRESS_A_BUTTON);
		initializeGameState();
		snakeState = SnakeState::WAITING;
		stateStartTime = get_absolute_time();
	}

	void enterSnakeMode(uint8_t initialPalette, uint8_t initialColor, int x, int y) {
		inSnakeMode = true;
		titleScreenDrawn = false;
		gameOverSequenceStarted = false;
		currentPalette = initialPalette;
		currentColor = initialColor;
		currentX = x;
		currentY = y;
		keyBuffer.clear(); // Clear main typing buffer
		while (!initialKeyCodeBuffer.empty()) { initialKeyCodeBuffer.pop(); } // Clear initials buffer
		initSnake();
		snakeState = SnakeState::NEUTRAL;
		stateStartTime = get_absolute_time();
		srand(to_ms_since_boot(get_absolute_time()));
	}

	// Helper for Buffered Initials
	void queueInitialDrawing(uint8_t keycode) {
		char initialChar = 'A' + (keycode - 0x10);
		const std::vector<std::pair<int, int>>* charVec = getCharVectorForKeycode(keycode);
		const std::vector<std::pair<int, int>>* underscoreVec = getUnderscoreVector(initialsEnteredCount);
		std::pair<int, int> offset = getCharOffset(initialsEnteredCount);

		// Ensure vectors are valid
		if (charVec && underscoreVec) {
			// Store the character locally until all 3 are entered
			enteredInitials[initialsEnteredCount] = initialChar;
			enteredInitials[initialsEnteredCount + 1] = '\0'; // Keep buffer null-terminated

			// Queue drawing commands
			drawPixels(*underscoreVec, 14); // Black out underscore
			drawPixelsAtOffset(*charVec, offset.first, offset.second, 6); // Draw char

			initialsEnteredCount++; // Increment count *after* queueing
		}
	}

	void processSnake(GCReport& report, uint64_t hold_duration_us) {
		if (!inSnakeMode) {
			 stateStartTime = 0;
			 report = defaultGcReport;
			 return;
		}
		if (stateStartTime == 0) {
			stateStartTime = get_absolute_time();
		}

		absolute_time_t currentTime = get_absolute_time();
		int64_t elapsed_us = absolute_time_diff_us(stateStartTime, currentTime);

		report = defaultGcReport;
		bool stateWillChange = false;

		// Determine stateWillChange based on current state (for timed actions)
		switch(snakeState) {
			case SnakeState::PRESS_A_BUTTON:
			case SnakeState::PRESS_L_BUTTON:
			case SnakeState::PRESS_R_BUTTON:
			case SnakeState::C_STICK_UP:
			case SnakeState::C_STICK_DOWN:
			case SnakeState::NEUTRAL:
				stateWillChange = elapsed_us >= (hold_duration_us * 2);
				break;
			case SnakeState::WAIT_FOR_START:
				stateWillChange = false;
				break;
			default: // Includes WAITING, MOVE_*
				stateWillChange = elapsed_us >= hold_duration_us;
				break;
		}

		// Process snake movement if game active and no drawing actions pending
		if (snakeState == SnakeState::WAITING && gameActive && movementQueue.empty()) {
			currentDirection = nextDirection;
			moveSnake(currentDirection); // This might queue drawing actions
		}


		switch (snakeState) {
			case SnakeState::PRESS_R_BUTTON: report.r = 1; report.analogR = 255; if (stateWillChange) { snakeState = SnakeState::NEUTRAL; stateStartTime = currentTime; } break;
			case SnakeState::PRESS_L_BUTTON: report.l = 1; report.analogL = 255; if (stateWillChange) { snakeState = SnakeState::NEUTRAL; stateStartTime = currentTime; } break;
			case SnakeState::MOVE_CURSOR_UP: report.yStick = 255; if (stateWillChange) { snakeState = SnakeState::NEUTRAL; stateStartTime = currentTime; } break;
			case SnakeState::MOVE_CURSOR_DOWN: report.yStick = 0; if (stateWillChange) { snakeState = SnakeState::NEUTRAL; stateStartTime = currentTime; } break;
			case SnakeState::MOVE_CURSOR_LEFT: report.xStick = 0; if (stateWillChange) { snakeState = SnakeState::NEUTRAL; stateStartTime = currentTime; } break;
			case SnakeState::MOVE_CURSOR_RIGHT: report.xStick = 255; if (stateWillChange) { snakeState = SnakeState::NEUTRAL; stateStartTime = currentTime; } break;
			case SnakeState::MOVE_CURSOR_UP_LEFT: report.xStick = 0; report.yStick = 255; if (stateWillChange) { snakeState = SnakeState::NEUTRAL; stateStartTime = currentTime; } break;
			case SnakeState::MOVE_CURSOR_UP_RIGHT: report.xStick = 255; report.yStick = 255; if (stateWillChange) { snakeState = SnakeState::NEUTRAL; stateStartTime = currentTime; } break;
			case SnakeState::MOVE_CURSOR_DOWN_LEFT: report.xStick = 0; report.yStick = 0; if (stateWillChange) { snakeState = SnakeState::NEUTRAL; stateStartTime = currentTime; } break;
			case SnakeState::MOVE_CURSOR_DOWN_RIGHT: report.xStick = 255; report.yStick = 0; if (stateWillChange) { snakeState = SnakeState::NEUTRAL; stateStartTime = currentTime; } break;
			case SnakeState::PRESS_A_BUTTON: report.a = 1; if (stateWillChange) { snakeState = SnakeState::NEUTRAL; stateStartTime = currentTime; } break; // Uses longer time check
			case SnakeState::C_STICK_UP: report.cyStick = 255; if (stateWillChange) { snakeState = SnakeState::NEUTRAL; stateStartTime = currentTime; } break; // Uses longer time check
			case SnakeState::C_STICK_DOWN: report.cyStick = 0; if (stateWillChange) { snakeState = SnakeState::NEUTRAL; stateStartTime = currentTime; } break; // Uses longer time check

			case SnakeState::NEUTRAL:
				// Provide a short delay between actions
				if (elapsed_us >= hold_duration_us) {
					snakeState = SnakeState::WAITING;
					stateStartTime = currentTime;
				}
				break;

			case SnakeState::WAITING:
				// Process the main action queue first
				if (!movementQueue.empty()) {
					snakeState = movementQueue.front();
					movementQueue.pop();
					stateStartTime = get_absolute_time(); // Reset timer for the new action
				}
				// If action queue is empty, check game state and initial buffer
				else {
					if (!titleScreenDrawn) {
						// Finished drawing title screen
						titleScreenDrawn = true;
						snakeState = SnakeState::WAIT_FOR_START;
						// No timer reset needed for WAIT_FOR_START
					} else if (!gameActive) {
						if (gameOverSequenceStarted) {
							// Game over drawing/input phase is active

							// Check if we need to process buffered initials (count < 3)
							if (isExpectingInitials() && !initialKeyCodeBuffer.empty()) {
								// We are expecting initials and have one buffered
								uint8_t nextInitialKc = initialKeyCodeBuffer.front();
								initialKeyCodeBuffer.pop();
								queueInitialDrawing(nextInitialKc); // Queues drawing commands
								// Stay in WAITING. Next loop will process the drawing commands.
								stateStartTime = get_absolute_time(); // Reset timer for this check cycle
							}
							// Check if exactly 3 initials are entered *and* drawing is complete
							else if (initialsEnteredCount == 3) {
								// We just finished drawing the 3rd initial (or buffer processing caught up)
								// Finalize the data, queue the cursor move, and mark finalization by incrementing count.
								strncpy(snakeHighScoreInitials, enteredInitials, 3); // Copy to persistent storage
								snakeHighScoreInitials[3] = '\0';
								enteredInitials[0] = '\0'; // Clear temporary buffer (optional, but good practice)
								navigateToPosition(31, 31); // Queue cursor move away
								initialsEnteredCount = 4;   // Increment count to signify finalization step is queued
								// Play a celebratory animation
								selectColor(1);
								selectPalette(5); selectPalette(4); selectPalette(3); selectPalette(6);
								// Stay in WAITING to process navigateToPosition
								stateStartTime = get_absolute_time();
							}
							// Check if finalization step (cursor move) is complete
							else if (initialsEnteredCount == 4) {
								// The navigateToPosition queued above must have just finished. Time to exit.
								snakeState = SnakeState::EXIT_SNAKE;
								stateStartTime = get_absolute_time();
							}
							// Check if it wasn't a high score case and drawing/cursor move is finished
							else if (!isExpectingInitials() && initialsEnteredCount == 0) {
								// This handles the non-high-score case where navigateToPosition finished.
								snakeState = SnakeState::EXIT_SNAKE;
								stateStartTime = get_absolute_time();
							}
							// Else: Waiting for more initials to be buffered, or for drawing actions to complete. Stay in WAITING.
							else {
								stateStartTime = get_absolute_time();
							}


						} else { // !gameOverSequenceStarted
							selectPalette(5); selectPalette(4); selectPalette(3); selectPalette(6);
							blanketFillWithColor(14);

							int currentScore = snakeSegments.size() * 100; // Example scoring

							if (currentScore > 0 && currentScore > snakeHighScore) {
								newHighScore = true;
								// HI SCORE (new)
								snakeHighScore = currentScore; // Update high score value
								drawPixels({{1,4},{1,5},{1,6},{1,7},{1,8},{2,6},{3,6},{4,4},{4,5},{4,6},{4,7},{4,8}},2); //H
								drawPixels({{6,4},{7,4},{8,4},{7,5},{7,6},{7,7},{7,8},{6,8},{8,8}},2); //I
								drawPixels({{12,4},{11,4},{10,5},{10,6},{11,6},{12,6},{12,7},{11,8},{10,8}},2); //S
								drawPixels({{16,4},{15,4},{14,5},{14,6},{14,7},{15,8},{16,8}},2); //C
								drawPixels({{19,4},{18,5},{18,6},{18,7},{19,8},{20,8},{21,7},{21,6},{21,5},{20,4}},2); //O
								drawPixels({{23,4},{23,5},{23,6},{23,7},{23,8},{24,4},{25,4},{26,5},{25,6},{24,6},{26,7},{26,8}},2); //R
								drawPixels({{28,4},{28,5},{28,6},{28,7},{28,8},{29,8},{30,8},{29,6},{30,6},{29,4},{30,4}},2); //E

								std::string scoreStr = std::to_string(snakeHighScore);
								int xPos = (scoreStr.length() == 4) ? 9 : (scoreStr.length() == 5) ? 7 : 5;
								drawString(xPos, 23, scoreStr.c_str(), 6); // Draw score value

								drawPixels({{11,19},{12,19},{13,19}},2); // _
								drawPixels({{15,19},{16,19},{17,19}},2); // _
								drawPixels({{19,19},{20,19},{21,19}},2); // _
								navigateToPosition(23,19);

								// Prepare for initial input phase
								initialsEnteredCount = 0;        // Reset counter
								enteredInitials[0] = '\0';       // Clear temporary buffer
								while (!initialKeyCodeBuffer.empty()) { initialKeyCodeBuffer.pop(); } // Clear shared buffer

							} else {
								// TOP (not a new high score)
								newHighScore = false;
								drawPixels({{10,4},{11,4},{12,4},{11,5},{11,6},{11,7},{11,8}},2); //T
								drawPixels({{15,4},{14,5},{14,6},{14,7},{15,8},{16,8},{17,7},{17,6},{17,5},{16,4}},2); //O
								drawPixels({{19,4},{19,5},{19,6},{19,7},{19,8},{20,4},{21,4},{22,5},{21,6},{20,6}},2); //P

								// Draw the existing high score initials
								drawString(11, 15, snakeHighScoreInitials, 6);

								std::string scoreStr = std::to_string(snakeHighScore); // Draw existing high score
								int xPos = (scoreStr.length() == 4) ? 9 : (scoreStr.length() == 5) ? 7 : 5;
								drawString(xPos, 23, scoreStr.c_str(), 6);

								navigateToPosition(31, 31); // Move cursor away
								initialsEnteredCount = 0; // Ensure count is 0 for exit check logic
							}

							gameOverSequenceStarted = true; // Mark drawing sequence as started
							// Stay in WAITING to process the drawing queue
							stateStartTime = get_absolute_time(); // Reset timer for the drawing actions
						}
					} 
					else {
						stateStartTime = get_absolute_time();
					}
				} 
				break;

			case SnakeState::WAIT_FOR_START:
				// Do nothing here. Report is already defaultGcReport.
				// State change is triggered externally by startGame() -> WAITING
				break;

			case SnakeState::EXIT_SNAKE:
				// Check timer using standard duration before actually exiting
				if (elapsed_us >= hold_duration_us) {
					inSnakeMode = false;
					gameActive = false;
					// Clear queues and state
					while (!movementQueue.empty()) { movementQueue.pop(); }
					while (!initialKeyCodeBuffer.empty()) { initialKeyCodeBuffer.pop(); }
					initialsEnteredCount = 0;
					enteredInitials[0] = '\0';
					gameOverSequenceStarted = false; // Reset for next game over
					keyBuffer.clear(); // Clear main typing buffer
					stateStartTime = 0; // Reset timer for next entry into snake mode
				}
				break;

			default:
				// Should not happen, reset to waiting
				snakeState = SnakeState::WAITING;
				stateStartTime = get_absolute_time();
				break;
		}
	} 

}