#include "townTunes.hpp"
#include "simulatedController.hpp"
#include "types.hpp"
#include "gcReport.hpp"
#include <cstdio>
#include <cstring>
#include <pico/stdlib.h>

bool isFrogCharacter(const Utf8Char& c) {
	// Compare with UTF-8 bytes for 🐸
	static const uint8_t frogChar[] = {0xF0, 0x9F, 0x90, 0xB8};
	if (c.length != 4) return false;
	return memcmp(c.bytes, frogChar, 4) == 0;
}

// External declarations
extern DeviceState device1;
extern DeviceState device2;
extern KeyBuffer keyBuffer;
extern bool tracker_is_active(KeyTracker* tracker, uint8_t keycode);

// Town tune mode variables
static int currentTuneIndex = 0;
static int currentNotePosition = 0;
static bool tuneSequenceStarted = false;
static bool tuneCompleted = false;  // Flag to track if a tune has completed playing
static int upPressCount = 0;
static bool pressingUp = true;
static bool inTownTuneMode = false;

static enum class TuneState {
	INIT_START,
	INIT_NEUTRAL,
	INIT_B,
	INIT_NEUTRAL2,
	INIT_Y,
	INIT_NEUTRAL3,
	INIT_A,
	INIT_NEUTRAL4,
	SETTING_NOTE,
	MOVING_RIGHT,
	X_BUTTON,
	PLAYING,
	WAITING,
	EXIT_START,  // State for Start button press
	EXIT_NEUTRAL1,
	EXIT_A1,
	EXIT_NEUTRAL2,
	EXIT_A2,
	EXIT_NEUTRAL3,
	EXIT_COMPLETE
} tuneState = TuneState::INIT_START;

namespace TownTunes {

    bool isInTownTuneMode() {
        return inTownTuneMode;
    }

    int getCurrentTuneIndex() {
        return currentTuneIndex;
    }

    bool isTuneCompleted() {
        return tuneCompleted;
    }

    void enterTownTuneMode() {
        // If we were previously in town tune mode and completed a tune,
        // advance to the next tune automatically
        if (inTownTuneMode == false && tuneCompleted == true) {
            // Move to the next tune (with wraparound)
            currentTuneIndex = (currentTuneIndex + 1) % TownTunes::tunes.size();
        }
        
        inTownTuneMode = true;
        tuneSequenceStarted = false;
        tuneCompleted = false;  // Reset completion flag
        currentNotePosition = 0;
        tuneState = TuneState::INIT_START;
        upPressCount = 0;       // Reset the counter
        pressingUp = true;      // Reset pressing state
        keyBuffer.clear();
    }

    void processTownTune(GCReport& report, bool xJustPressed, bool leftJustPressed, bool rightJustPressed,
                        bool startPressed, int64_t hold_duration_us) {
        static absolute_time_t stateStartTime = get_absolute_time();
        absolute_time_t currentTime = get_absolute_time();
        int64_t elapsed_us = absolute_time_diff_us(stateStartTime, currentTime);
        
        // Standard duration and twice the standard duration for longer waits
        const int64_t standard_duration = hold_duration_us;
        const int64_t longer_duration = 14 * standard_duration;
        const int64_t tune_playtime = 4200000;
        
        // Start with neutral state
        report = defaultGcReport;
        
        bool stateWillChange = false;
        
        // Different timing requirements for different states
        switch (tuneState) {
            case TuneState::INIT_NEUTRAL:
            case TuneState::INIT_NEUTRAL2:
            case TuneState::INIT_NEUTRAL3:
            case TuneState::INIT_NEUTRAL4:
            case TuneState::EXIT_NEUTRAL1:
            case TuneState::EXIT_NEUTRAL2:
            case TuneState::EXIT_NEUTRAL3:
                stateWillChange = elapsed_us >= longer_duration;
                break;
            case TuneState::PLAYING:
                stateWillChange = elapsed_us >= tune_playtime;
                break;
            default:
                stateWillChange = elapsed_us >= standard_duration;
                break;
        }
        
        if (!tuneSequenceStarted) {
            tuneSequenceStarted = true;
            stateStartTime = currentTime;
            return;
        }
        
        switch (tuneState) {
            case TuneState::INIT_START:
                // Normal tune mode initialization
                report.start = 1;
                if (stateWillChange) {
                    tuneState = TuneState::INIT_NEUTRAL;
                    stateStartTime = currentTime;
                }
                break;
                
            case TuneState::EXIT_START:
                // Start button press for exit sequence
                report.start = 1;
                if (stateWillChange) {
                    tuneState = TuneState::EXIT_NEUTRAL1;
                    stateStartTime = currentTime;
                }
                break;
                
            case TuneState::INIT_NEUTRAL:
                // Neutral state with longer duration
                if (stateWillChange) {
                    tuneState = TuneState::INIT_B;
                    stateStartTime = currentTime;
                }
                break;
                
            case TuneState::INIT_B:
                report.b = 1;
                if (stateWillChange) {
                    tuneState = TuneState::INIT_NEUTRAL2;
                    stateStartTime = currentTime;
                }
                break;
                
            case TuneState::INIT_NEUTRAL2:
                // Neutral state with double duration
                if (stateWillChange) {
                    tuneState = TuneState::INIT_Y;
                    stateStartTime = currentTime;
                }
                break;
                
            case TuneState::INIT_Y:
                report.y = 1;
                if (stateWillChange) {
                    tuneState = TuneState::INIT_NEUTRAL3;
                    stateStartTime = currentTime;
                }
                break;
                
            case TuneState::INIT_NEUTRAL3:
                // Neutral state with double duration
                if (stateWillChange) {
                    tuneState = TuneState::INIT_A;
                    stateStartTime = currentTime;
                }
                break;
                
            case TuneState::INIT_A:
                report.a = 1;
                if (stateWillChange) {
                    tuneState = TuneState::INIT_NEUTRAL4;
                    stateStartTime = currentTime;
                }
                break;
                
            case TuneState::INIT_NEUTRAL4:
                // Neutral state with double duration
                if (stateWillChange) {
                    tuneState = TuneState::SETTING_NOTE;
                    stateStartTime = currentTime;
                }
                break;
                
            case TuneState::SETTING_NOTE: {
                // Determine the target note and current note (always start from silent "_")
                char targetNote = TownTunes::tunes[currentTuneIndex][currentNotePosition];
                int targetNoteIndex = TownTunes::getNoteIndex(targetNote);
                
                if (pressingUp && upPressCount < targetNoteIndex) {
                    // Press Up to change note
                    report.yStick = 255;
                    if (stateWillChange) {
                        upPressCount++;
                        pressingUp = false;
                        stateStartTime = currentTime;
                    }
                } else if (!pressingUp && upPressCount < targetNoteIndex) {
                    // Return to neutral between presses
                    if (stateWillChange) {
                        pressingUp = true;
                        stateStartTime = currentTime;
                    }
                } else {
                    // We've reached the target note, move to the next state
                    if (stateWillChange) {
                        upPressCount = 0;
                        tuneState = TuneState::MOVING_RIGHT;
                        stateStartTime = currentTime;
                    }
                }
                break;
            }
                
            case TuneState::MOVING_RIGHT:
                // Handle reaching the last note position
                if (currentNotePosition == 15) {
                    // Go to separate X button state
                    if (stateWillChange) {
                        tuneState = TuneState::X_BUTTON;
                        stateStartTime = currentTime;
                    }
                } else {
                    // Move right to next note position
                    report.xStick = 255;
                    if (stateWillChange) {
                        currentNotePosition++;
                        tuneState = TuneState::SETTING_NOTE;
                        stateStartTime = currentTime;
                    }
                }
                break;
                
            case TuneState::X_BUTTON:
                // Press X button once for final confirmation
                report.x = 1;
                if (stateWillChange) {
                    tuneState = TuneState::PLAYING;
                    stateStartTime = currentTime;
                }
                break;
                
            case TuneState::PLAYING:
                // Wait for the tune play out
                if (stateWillChange) {
                    currentNotePosition = 0;
                    tuneSequenceStarted = false;
                    tuneState = TuneState::WAITING;
                    tuneCompleted = true;  // Mark tune as completed
                    stateStartTime = currentTime;
                }
                break;
                
            case TuneState::WAITING:
                // Just wait in neutral state until user selects next action
                break;
                
            // Exit sequence states
            case TuneState::EXIT_NEUTRAL1:
                // Neutral state with longer duration after start press
                if (stateWillChange) {
                    tuneState = TuneState::EXIT_A1;
                    stateStartTime = currentTime;
                }
                break;
                
            case TuneState::EXIT_A1:
                // First A press in exit sequence
                report.a = 1;
                if (stateWillChange) {
                    tuneState = TuneState::EXIT_NEUTRAL2;
                    stateStartTime = currentTime;
                }
                break;
                
            case TuneState::EXIT_NEUTRAL2:
                // Neutral state with longer duration between A presses
                if (stateWillChange) {
                    tuneState = TuneState::EXIT_A2;
                    stateStartTime = currentTime;
                }
                break;
                
            case TuneState::EXIT_A2:
                // Second A press in exit sequence
                report.a = 1;
                if (stateWillChange) {
                    tuneState = TuneState::EXIT_NEUTRAL3;
                    stateStartTime = currentTime;
                }
                break;
                
            case TuneState::EXIT_NEUTRAL3:
                // Final neutral state
                if (stateWillChange) {
                    tuneState = TuneState::EXIT_COMPLETE;
                    stateStartTime = currentTime;
                }
                break;
                
            case TuneState::EXIT_COMPLETE:
                // Exit town tune mode but maintain currentTuneIndex and tuneCompleted status
                inTownTuneMode = false;
                tuneState = TuneState::INIT_START; // Reset for next time
                currentNotePosition = 0;
                tuneSequenceStarted = false;
                break;
        }
        
        // Handle input events for navigation
        bool canNavigate = (tuneState == TuneState::SETTING_NOTE || 
                            tuneState == TuneState::MOVING_RIGHT || 
                            tuneState == TuneState::WAITING);
                            
        if (canNavigate) {
            // Handle X button press in WAITING state
            if (tuneState == TuneState::WAITING && xJustPressed) {
                // Transition to X_BUTTON state (same as used in automatic sequence)
                tuneState = TuneState::X_BUTTON;
                tuneSequenceStarted = false;  // Force reset of state timer
            }

            // Check for left/right to page between tunes
            if (leftJustPressed) {  // Left pressed
                // Go to previous tune (with wraparound)
                if (currentTuneIndex > 0) {
                    currentTuneIndex--;
                } else {
                    currentTuneIndex = TownTunes::tunes.size() - 1;
                }
                // Reset tune state
                tuneSequenceStarted = false;
                tuneCompleted = false;
                currentNotePosition = 0;
                upPressCount = 0;       // Reset the counter
                pressingUp = true;      // Reset pressing state 
                tuneState = TuneState::INIT_START;
            } else if (rightJustPressed) {  // Right pressed
                // Go to next tune (with wraparound)
                currentTuneIndex = (currentTuneIndex + 1) % TownTunes::tunes.size();
                // Reset tune state
                tuneSequenceStarted = false;
                tuneCompleted = false;
                currentNotePosition = 0;
                upPressCount = 0;       // Reset the counter
                pressingUp = true;      // Reset pressing state
                tuneState = TuneState::INIT_START;
            }
        }
        
        // Allow START button for exit in all states except PLAYING
        if (tuneState != TuneState::PLAYING && startPressed) {  // Start pressed - initiate exit sequence
            // Start exit sequence
            tuneState = TuneState::EXIT_START;  // Use dedicated exit state
            tuneSequenceStarted = false;  // Force reset of state timer
        }
    }

    void exitTownTuneMode() {
        inTownTuneMode = false;
        tuneState = TuneState::INIT_START; // Reset for next time
        currentNotePosition = 0;
        tuneSequenceStarted = false;
    }
}
