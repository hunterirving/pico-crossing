#pragma once
#ifndef SNAKE_HPP
#define SNAKE_HPP

#include <vector>
#include <deque>
#include <cstdint>
#include <queue>
#include "gcReport.hpp"
#include "types.hpp"


namespace Snake {
	// Character coordinate vectors for alphanumerics
	// Letters
	const std::vector<std::pair<int, int>> charA = {{0,4},{0,3},{0,2},{0,1},{1,0},{2,1},{2,2},{2,3},{2,4},{0,2},{1,2},{2,2}};
	const std::vector<std::pair<int, int>> charB = {{0,0},{0,1},{0,2},{0,3},{0,4},{1,0},{2,1},{1,2},{2,3},{1,4}};
	const std::vector<std::pair<int, int>> charC = {{2,0},{1,0},{0,1},{0,2},{0,3},{1,4},{2,4}};
	const std::vector<std::pair<int, int>> charD = {{0,0},{0,1},{0,2},{0,3},{0,4},{1,0},{2,1},{2,2},{2,3},{1,4}};
	const std::vector<std::pair<int, int>> charE = {{0,0},{0,1},{0,2},{0,3},{0,4},{1,4},{2,4},{1,2},{1,0},{2,0}};
	const std::vector<std::pair<int, int>> charF = {{0,0},{0,1},{0,2},{0,3},{0,4},{1,0},{2,0},{1,2}};
	const std::vector<std::pair<int, int>> charG = {{2,0},{1,0},{0,1},{0,2},{0,3},{1,4},{2,4},{2,3},{2,2}};
	const std::vector<std::pair<int, int>> charH = {{0,0},{0,1},{0,2},{0,3},{0,4},{0,2},{1,2},{2,2},{2,0},{2,1},{2,3},{2,4}};
	const std::vector<std::pair<int, int>> charI = {{1,0},{1,1},{1,2},{1,3},{1,4},{0,0},{2,0},{0,4},{2,4}};
	const std::vector<std::pair<int, int>> charJ = {{2,0},{2,1},{2,2},{2,3},{1,4},{0,3}};
	const std::vector<std::pair<int, int>> charK = {{0,0},{0,1},{0,2},{0,3},{0,4},{2,0},{2,1},{1,2},{2,3},{2,4}};
	const std::vector<std::pair<int, int>> charL = {{0,0},{0,1},{0,2},{0,3},{0,4},{1,4},{2,4}};
	const std::vector<std::pair<int, int>> charM = {{0,0},{0,1},{0,2},{0,3},{0,4},{1,1},{1,2},{2,0},{2,1},{2,2},{2,3},{2,4}};
	const std::vector<std::pair<int, int>> charN = {{0,0},{0,1},{0,2},{0,3},{0,4},{1,0},{2,0},{2,1},{2,2},{2,3},{2,4}};
	const std::vector<std::pair<int, int>> charO = {{1,0},{0,1},{0,2},{0,3},{1,4},{2,3},{2,2},{2,1}};
	const std::vector<std::pair<int, int>> charP = {{0,0},{0,1},{0,2},{0,3},{0,4},{1,0},{2,0},{2,1},{2,2},{1,2}};
	const std::vector<std::pair<int, int>> charQ = {{1,0},{0,1},{0,2},{0,3},{1,4},{2,2},{2,1},{2,0},{1,3},{2,4}};
	const std::vector<std::pair<int, int>> charR = {{0,0},{0,1},{0,2},{0,3},{0,4},{1,0},{2,0},{2,1},{1,2},{2,3},{2,4}};
	const std::vector<std::pair<int, int>> charS = {{2,0},{1,0},{0,1},{0,2},{1,2},{2,2},{2,3},{1,4},{0,4}};
	const std::vector<std::pair<int, int>> charT = {{0,0},{1,0},{2,0},{1,1},{1,2},{1,3},{1,4}};
	const std::vector<std::pair<int, int>> charU = {{0,0},{0,1},{0,2},{0,3},{0,4},{1,4},{2,4},{2,3},{2,2},{2,1},{2,0}};
	const std::vector<std::pair<int, int>> charV = {{0,0},{0,1},{0,2},{1,3},{1,4},{2,2},{2,1},{2,0}};
	const std::vector<std::pair<int, int>> charW = {{0,0},{0,1},{0,2},{0,3},{0,4},{1,3},{1,2},{2,4},{2,3},{2,2},{2,1},{2,0}};
	const std::vector<std::pair<int, int>> charX = {{0,0},{0,1},{1,2},{2,3},{2,4},{2,0},{2,1},{0,3},{0,4}};
	const std::vector<std::pair<int, int>> charY = {{0,0},{0,1},{1,2},{2,1},{2,0},{1,3},{1,4}};
	const std::vector<std::pair<int, int>> charZ = {{0,0},{1,0},{2,0},{2,1},{1,2},{0,3},{0,4},{1,4},{2,4}};
	
	// Digits
	const std::vector<std::pair<int, int>> digit0 = {{1,0},{0,0},{0,1},{0,2},{0,3},{0,4},{1,4},{2,4},{2,3},{2,2},{2,1},{2,0}};
	const std::vector<std::pair<int, int>> digit1 = {{0,0},{1,0},{1,1},{1,2},{1,3},{1,4},{0,4},{2,4}};
	const std::vector<std::pair<int, int>> digit2 = {{0,0},{1,0},{2,0},{2,1},{2,2},{1,2},{0,2},{0,3},{0,4},{1,4},{2,4}};
	const std::vector<std::pair<int, int>> digit3 = {{0,0},{1,0},{2,0},{2,1},{1,2},{2,2},{2,3},{2,4},{1,4},{0,4}};
	const std::vector<std::pair<int, int>> digit4 = {{0,0},{0,1},{0,2},{1,2},{2,2},{2,0},{2,1},{2,3},{2,4}};
	const std::vector<std::pair<int, int>> digit5 = {{2,0},{1,0},{0,0},{0,1},{0,2},{1,2},{2,2},{2,3},{2,4},{1,4},{0,4}};
	const std::vector<std::pair<int, int>> digit6 = {{2,0},{1,0},{0,0},{0,1},{0,2},{0,3},{0,4},{1,4},{2,4},{2,3},{2,2},{1,2}};
	const std::vector<std::pair<int, int>> digit7 = {{0,0},{1,0},{2,0},{2,1},{2,2},{2,3},{2,4}};
	const std::vector<std::pair<int, int>> digit8 = {{1,0},{0,0},{0,1},{0,2},{1,2},{2,2},{2,1},{2,0},{0,3},{0,4},{1,4},{2,4},{2,3}};
	const std::vector<std::pair<int, int>> digit9 = {{2,0},{1,0},{0,0},{0,1},{0,2},{1,2},{2,2},{2,1},{2,3},{2,4},{1,4},{0,4}};

	// Enum for snake mode state machine
	enum class SnakeState {
		// Navigation states for cursor movement
		MOVE_CURSOR_UP,
		MOVE_CURSOR_DOWN,
		MOVE_CURSOR_LEFT,
		MOVE_CURSOR_RIGHT,
		MOVE_CURSOR_UP_LEFT,
		MOVE_CURSOR_UP_RIGHT,
		MOVE_CURSOR_DOWN_LEFT,
		MOVE_CURSOR_DOWN_RIGHT,
		// Press A to confirm
		PRESS_A_BUTTON,
		// For changing colors
		C_STICK_UP,
		C_STICK_DOWN,
		// For changing between tools, canvas, palette menu
		PRESS_R_BUTTON,
		PRESS_L_BUTTON,
		// Other
		NEUTRAL,
		WAITING,
		WAIT_FOR_START,
		EXIT_SNAKE,
	};

	// Direction enum
	enum class Direction {
		UP,
		DOWN,
		LEFT,
		RIGHT
	};

	// Tile type enum for game board
	enum class TileType {
		EMPTY,
		SNAKE_BODY,
		APPLE
	};

	// Position struct for snake segments
	struct Position {
		int x;
		int y;
		
		bool operator==(const Position& other) const {
			return x == other.x && y == other.y;
		}
	};

	// Public functions
	bool isInSnakeMode();
	void enterSnakeMode(uint8_t initialPalette, uint8_t initialColor, int x, int y);
	void processSnake(GCReport& report, uint64_t hold_duration_us);
	void updateSnakeDirection(); // New function to capture direction changes at any time
	bool isExpectingInitials();

	SnakeState getCurrentState(); // Allows external check of the current state
	void startGame();           // Called externally when Start is pressed

	// Internal helper functions
	void selectPalette(uint8_t targetPalette);
	void selectColor(uint8_t colorId);
	void navigateToPosition(int x, int y);
	void drawPixels(const std::vector<std::pair<int, int>>& coordinates, uint8_t colorId);
	void drawPixelsAtOffset(const std::vector<std::pair<int, int>>& coordinates, int offsetX, int offsetY, uint8_t colorId);
	void drawPixel(int x, int y, uint8_t colorId);
	void drawString(int x, int y, const char* str, uint8_t colorId);
	void initSnake(); // Queues initial title screen drawing
	
	// Snake game logic functions
	void moveSnake(Direction dir);
	bool checkCollision(const Position& position);
	void placeApple();
	bool checkAppleCollision(const Position& position);
}

extern std::queue<uint8_t> initialKeyCodeBuffer;

#endif