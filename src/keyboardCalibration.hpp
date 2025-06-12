namespace KeyboardCalibration {
	struct CalibMove {
		uint8_t xStick;
		uint8_t yStick;
	};
	
	static const CalibMove sequence[] = {
		{0, 128},    // left
		{128, 255},  // up
		{0, 128},    // left
		{128, 255},  // up
		{0, 128},    // left
		{128, 255},  // up
		{0, 128},    // left
		{128, 128},  // neutral
		{0, 128},    // left
		{128, 128},  // neutral
		{0, 128},    // left
		{128, 128},  // neutral
		{0, 128},    // left
		{128, 128},  // neutral
		{0, 128},    // left
		{128, 128},  // neutral
		{0, 128}     // final left
	};
	
	static size_t currentMove = 0;
	
	inline void reset() {
		currentMove = 0;
	}
	
	inline bool isComplete() {
		return currentMove >= sizeof(sequence) / sizeof(sequence[0]);
	}
	
	inline const CalibMove& getCurrentMove() {
		return sequence[currentMove];
	}
	
	inline void advance() {
		currentMove++;
	}
}