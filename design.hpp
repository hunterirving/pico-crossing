#ifndef DESIGN_HPP
#define DESIGN_HPP

#include <vector>
#include <cstdint>
#include <cstring>
#include "gcReport.hpp"
#include "types.hpp"

// Function to check if a UTF-8 character is the paint emoji
bool isPaintCharacter(const Utf8Char& c);

namespace Design {
	// Color structure
	struct FrameData {
		uint8_t paletteId;           // 0-15, representing the in-game palette to use
		uint8_t pixels[32][32];      // 32x32 grid of color indexes (0-14 for the 15 colors)
	};

	// Forward declaration for frame provider
	class FrameProvider;
	
	// Frameset structure
	struct Frameset {
		std::vector<FrameData> frames;
		size_t currentFrameIndex;
		FrameProvider* provider;  // Optional provider for streaming frames
		
		Frameset() : currentFrameIndex(0), provider(nullptr) {}
	};
	
	// Abstract base class for providing frames
	class FrameProvider {
	public:
		virtual ~FrameProvider() = default;
		virtual size_t getFrameCount() const = 0;
		virtual const FrameData& getFrame(size_t index) = 0;
		virtual uint8_t getPaletteId(size_t index) const = 0;
	};
	
	// Streaming frame provider for large framesets
	class StreamingFrameProvider : public FrameProvider {
	private:
		const uint8_t* frameData;     // Pointer to flash-stored frame data
		size_t frameCount;
		mutable FrameData currentFrame;  // Single frame buffer for current frame
		mutable size_t cachedFrameIndex; // Which frame is currently cached
		
	public:
		StreamingFrameProvider(const uint8_t* data, size_t count) 
			: frameData(data), frameCount(count), cachedFrameIndex(SIZE_MAX) {}
		
		size_t getFrameCount() const override { return frameCount; }
		uint8_t getPaletteId(size_t index) const override;
		const FrameData& getFrame(size_t index) override;
	};
		
	// Enum for design mode state machine
	enum class DesignState {
		INIT_CALIBRATE,
		CALIBRATE_NEUTRAL,
		MOVE_TO_PALETTE_MENU,
		PALETTE_MENU_NEUTRAL,
		PALETTE_MENU_NAVIGATION,
		PALETTE_NAV_NEUTRAL,
		CHANGE_PALETTE_BUTTON,
		PALETTE_BUTTON_NEUTRAL,
		RETURN_TO_CANVAS,
		RETURN_CANVAS_NEUTRAL,
		SELECT_COLOR,
		SELECT_COLOR_NEUTRAL,
		DRAW_PIXEL,
		DRAW_PIXEL_NEUTRAL,
		MOVE_CURSOR,
		MOVE_CURSOR_NEUTRAL,
		NEXT_FRAME,
		FRAME_LOADING_SETTLING,
		EXIT_DESIGN,
		EXIT_NEUTRAL,
		WAITING
	};

	// Public interface functions
	bool isInDesignMode();
	
	void enterDesignMode();
	
	void processDesign(GCReport& report, uint64_t hold_duration_us);
	
	void exitDesignMode();
	
	// Initialize with the checkerboard pattern
	void initDefaultFrameset();
	
	// Initialize frameset
	void initFrameset();
	
	// Get current frameset
	Frameset& getCurrentFrameset();
	
	// Helper functions for frame access
	size_t getFrameCount();
	const FrameData& getCurrentFrame();
	uint8_t getCurrentPaletteId();

	// Initialize with a generated streaming frameset from an image
	// Generated at: 2025-06-09 00:06:58
	// Source file: hunter.jpg
	// Frame count: 1 frame (using streaming mode)
	
	// Frame data stored in flash memory (format: paletteId followed by 1024 pixel bytes per frame)
	static const uint8_t streamingFrameData[] = {
		4, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 8, 7, 7, 7, 7, 7, 7, 8, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 8, 7, 7, 7, 7, 7, 7, 7, 8, 8, 8, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 8, 8, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 8, 8, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 8, 7, 7, 7, 7, 7, 6, 6, 6, 6, 6, 6, 7, 7, 7, 7, 8, 8, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 8, 8, 7, 7, 6, 6, 12, 12, 12, 12, 6, 6, 6, 6, 6, 6, 6, 7, 7, 8, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 8, 8, 7, 7, 7, 6, 12, 12, 12, 12, 12, 6, 6, 6, 6, 6, 6, 6, 6, 6, 8, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 8, 7, 7, 7, 6, 12, 12, 12, 12, 12, 12, 12, 12, 6, 6, 7, 7, 6, 6, 6, 6, 7, 8, 14, 14, 14, 14, 14, 14, 14, 14, 14, 8, 7, 6, 6, 6, 6, 6, 6, 6, 13, 12, 7, 7, 7, 7, 8, 8, 8, 7, 6, 12, 7, 8, 14, 14, 14, 14, 14, 14, 14, 14, 8, 7, 6, 6, 6, 6, 6, 6, 6, 12, 13, 12, 7, 7, 8, 8, 7, 6, 7, 7, 7, 12, 12, 7, 7, 14, 14, 14, 14, 14, 14, 14, 7, 7, 6, 6, 12, 6, 7, 7, 6, 12, 6, 12, 12, 6, 7, 6, 12, 12, 6, 6, 7, 6, 13, 12, 6, 8, 14, 14, 14, 14, 14, 14, 7, 7, 6, 12, 6, 8, 7, 7, 6, 6, 7, 6, 13, 6, 12, 12, 7, 7, 7, 8, 7, 6, 12, 6, 6, 8, 14, 14, 14, 14, 14, 8, 8, 6, 12, 13, 6, 8, 7, 6, 12, 12, 6, 6, 6, 8, 6, 6, 7, 7, 6, 7, 8, 6, 12, 12, 6, 8, 14, 14, 14, 14, 14, 8, 7, 12, 12, 13, 6, 7, 7, 7, 7, 6, 6, 6, 8, 14, 7, 6, 7, 7, 7, 6, 7, 6, 12, 13, 6, 8, 14, 14, 14, 14, 14, 7, 7, 6, 13, 13, 7, 8, 8, 8, 8, 7, 7, 6, 14, 14, 14, 7, 8, 8, 8, 8, 8, 7, 12, 13, 6, 14, 14, 14, 14, 14, 14, 6, 12, 6, 13, 13, 8, 14, 8, 14, 14, 8, 6, 8, 14, 14, 14, 8, 8, 14, 14, 8, 14, 7, 13, 13, 6, 14, 14, 14, 14, 14, 8, 12, 12, 6, 12, 6, 14, 14, 14, 8, 8, 7, 8, 14, 14, 14, 14, 14, 8, 8, 14, 14, 14, 8, 12, 13, 7, 14, 14, 14, 14, 14, 8, 12, 6, 6, 12, 7, 14, 14, 14, 14, 14, 14, 14, 7, 6, 7, 6, 8, 14, 8, 8, 14, 14, 14, 6, 13, 7, 8, 14, 14, 14, 14, 8, 6, 12, 12, 13, 7, 14, 14, 14, 14, 14, 14, 8, 12, 6, 7, 12, 6, 14, 14, 14, 14, 14, 14, 7, 13, 12, 7, 14, 14, 14, 14, 14, 7, 12, 12, 13, 8, 14, 14, 14, 14, 14, 14, 8, 7, 7, 7, 7, 7, 14, 14, 14, 14, 14, 14, 7, 13, 12, 8, 14, 14, 14, 14, 14, 7, 13, 13, 13, 8, 14, 14, 14, 14, 14, 14, 14, 14, 8, 8, 14, 14, 14, 14, 14, 14, 14, 14, 6, 13, 6, 14, 14, 14, 14, 14, 8, 6, 13, 13, 13, 7, 14, 14, 14, 14, 14, 14, 8, 7, 7, 7, 7, 8, 14, 14, 14, 14, 14, 14, 6, 13, 6, 14, 14, 14, 14, 14, 8, 12, 13, 13, 13, 8, 14, 14, 14, 14, 8, 7, 12, 12, 7, 7, 6, 12, 7, 14, 14, 14, 14, 14, 11, 13, 6, 14, 14, 14, 14, 14, 14, 7, 13, 13, 6, 14, 14, 14, 14, 8, 12, 13, 6, 8, 14, 14, 8, 7, 13, 7, 14, 14, 14, 14, 6, 13, 7, 14, 14, 14, 14, 14, 14, 8, 13, 13, 8, 14, 14, 14, 14, 14, 8, 7, 8, 7, 8, 8, 7, 7, 6, 8, 14, 14, 14, 14, 8, 13, 7, 14, 14, 14, 14, 14, 14, 14, 7, 12, 12, 6, 14, 14, 14, 14, 14, 14, 8, 8, 8, 8, 8, 8, 14, 14, 14, 14, 14, 14, 7, 6, 14, 14, 14, 14, 14, 14, 14, 14, 8, 12, 13, 12, 14, 14, 14, 14, 14, 14, 8, 7, 6, 6, 7, 8, 14, 14, 14, 14, 14, 7, 12, 8, 14, 14, 14, 14, 14, 14, 14, 14, 14, 7, 13, 12, 14, 14, 14, 14, 14, 14, 14, 8, 7, 7, 8, 14, 14, 14, 14, 14, 14, 11, 7, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 6, 13, 7, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 8, 12, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 6, 6, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 8, 8, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 8, 8, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 8, 7, 7, 7, 7, 7, 8, 8, 8, 8, 8, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 8, 7, 6, 6, 6, 7, 7, 8, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14
	};
	
	static StreamingFrameProvider streamingProvider(streamingFrameData, 1);
	
	inline void initGeneratedFrameset() {
		// Get reference to the frameset
		Frameset& fs = getCurrentFrameset();
		
		// Clear any existing frames and set up streaming provider
		fs.frames.clear();
		fs.provider = &streamingProvider;
		fs.currentFrameIndex = 0;
	}
}

#endif