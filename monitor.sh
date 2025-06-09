#!/bin/bash
set -e

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

# Detect operating system
OS="$(uname)"

# Function to find the appropriate serial port
find_serial_port() {
	case $OS in
		"Darwin")  # macOS
			ls /dev/tty.usbmodem* 2>/dev/null | head -n 1
			;;
		"Linux")
			ls /dev/ttyACM* 2>/dev/null | head -n 1
			;;
		"MINGW"*|"MSYS"*|"CYGWIN"*)  # Windows
			ls /dev/ttyS* 2>/dev/null | head -n 1
			;;
	esac
}

# Find the serial port
SERIAL_PORT=$(find_serial_port)

if [ -n "$SERIAL_PORT" ]; then
	echo "Opening serial connection to $SERIAL_PORT"
	case $OS in
		"Darwin"|"Linux")
			if command -v minicom &> /dev/null; then
				minicom -D $SERIAL_PORT -b 115200
			else
				echo "minicom not found. Please install minicom to open serial connection."
			fi
			;;
		"MINGW"*|"MSYS"*|"CYGWIN"*)
			if command -v putty &> /dev/null; then
				putty -serial $SERIAL_PORT -sercfg 115200
			else
				echo "PuTTY not found. Please install PuTTY to open serial connection on Windows."
			fi
			;;
	esac
else
	echo "No suitable serial port found. Is your Pico connected?"
fi