#include "nookCodes.hpp"
#include "simulatedController.hpp"
#include "types.hpp"
#include <cstdio>
#include <cstdlib>
#include <cctype>
#include <string>
#include <cstring>
#include <vector>

// Static variables for nook code mode
static bool inNookCodeMode = false;
static std::vector<Utf8Char> itemName;
static bool needToClearBuffer = false;
static bool needToPressStart = false;
static const size_t MAX_ITEM_NAME_LENGTH = 28;

// External references
extern KeyBuffer keyBuffer;

// Utility functions for string and UTF-8 handling
std::string utf8CharsToString(const std::vector<Utf8Char>& chars) {
	std::string result;
	for (const auto& c : chars) {
		result.append(reinterpret_cast<const char*>(c.bytes), c.length);
	}
	return result;
}

void convertStringToUtf8Chars(const std::string& str, std::vector<Utf8Char>& chars) {
	size_t i = 0;
	while (i < str.length()) {
		Utf8Char c;
		size_t charLen = 1;
		
		// Determine UTF-8 character length
		if ((str[i] & 0x80) == 0) charLen = 1;
		else if ((str[i] & 0xE0) == 0xC0) charLen = 2;
		else if ((str[i] & 0xF0) == 0xE0) charLen = 3;
		else if ((str[i] & 0xF8) == 0xF0) charLen = 4;
		
		c.length = charLen;
		for (size_t j = 0; j < charLen && i < str.length(); j++) {
			c.bytes[j] = str[i++];
		}
		chars.push_back(c);
	}
}

bool isKeyCharacter(const Utf8Char& c) {
	// Compare with UTF-8 bytes for 🔑
	static const uint8_t keyChar[] = {0xF0, 0x9F, 0x94, 0x91};
	if (c.length != 4) return false;
	return memcmp(c.bytes, keyChar, 4) == 0;
}

namespace NookCodes {

	bool isInNookCodeMode() {
		return inNookCodeMode;
	}
	
	bool needsBackspace() {
		return false; // This is handled in the simulatedController.cpp now
	}
	
	bool shouldClearBuffer() {
		return needToClearBuffer;
	}
	
	bool shouldPressStart() {
		return needToPressStart;
	}
	
	void clearNeedToClearBuffer() {
		needToClearBuffer = false;
	}
	
	void clearNeedToPressStart() {
		needToPressStart = false;
	}
	
	std::vector<Utf8Char>& getItemName() {
		return itemName;
	}

	void enterNookCodeMode() {
		inNookCodeMode = false; // Reset first to avoid potential issues
		inNookCodeMode = true;
		itemName.clear();
		keyBuffer.clear();
	}

	void exitNookCodeMode() {
		inNookCodeMode = false;
		itemName.clear();
	}

	bool processBackspace() {
		if (!itemName.empty()) {
			itemName.pop_back();
			return true; // Need backspace action
		}
		return false;
	}

	void addCharToItemName(const Utf8Char& c) {
		if (itemName.size() < MAX_ITEM_NAME_LENGTH) {
			itemName.push_back(c);
		}
	}

	bool checkAndProcessNookCode() {
		std::string currentItem = utf8CharsToString(itemName);
		// Convert to lowercase and replace special space character (␣, UTF-8: E2 90 A3)
		std::string processedItem;
		for (size_t i = 0; i < currentItem.length(); i++) {
			// Check for UTF-8 sequence of ␣ (E2 90 A3)
			if (i + 2 < currentItem.length() && 
				(unsigned char)currentItem[i] == 0xE2 && 
				(unsigned char)currentItem[i+1] == 0x90 && 
				(unsigned char)currentItem[i+2] == 0xA3) {
				processedItem += ' ';
				i += 2;  // Skip the next two bytes since we consumed them
			} else {
				processedItem += std::tolower(currentItem[i]);
			}
		}
			
		const char* code = NookCodes::find_code(processedItem.c_str());
		if (code != nullptr) {
			// Set flag to clear buffer first
			needToClearBuffer = true;
			// Store the code to be processed after clearing
			std::vector<Utf8Char> codeChars;
			convertStringToUtf8Chars(code, codeChars);
			keyBuffer.clear();
			for (const auto& c : codeChars) {
				keyBuffer.push(c);
			}
			needToPressStart = true;
			inNookCodeMode = false;
			itemName.clear();
			return true;
		}
		return false;
	}
}
