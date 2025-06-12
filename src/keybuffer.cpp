#include "types.hpp"

bool KeyBuffer::push(const Utf8Char& c) {
	if (count >= BUFFER_SIZE) return false;
	buffer[writePos] = c;
	writePos = (writePos + 1) % BUFFER_SIZE;
	count++;
	return true;
}

bool KeyBuffer::pop(Utf8Char& c) {
	if (count == 0) return false;
	c = buffer[readPos];
	readPos = (readPos + 1) % BUFFER_SIZE;
	count--;
	return true;
}

bool KeyBuffer::isEmpty() const {
	return count == 0;
}
