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

# Flash using picotool
if command -v picotool &> /dev/null; then
	echo "Attempting to flash with picotool..."
	
	# Try to force reboot into BOOTSEL mode if device is running
	picotool reboot -f -u

	# Wait a moment for the device to reboot
	sleep 2

	# Now try to load the program
	picotool load "$SCRIPT_DIR/build/gamecube_controller_reader.uf2"

	# Handle proper unmounting based on OS
	case $OS in
		"Darwin")  # macOS
			if [ -d "/Volumes/RPI-RP2" ]; then
				diskutil unmount "/Volumes/RPI-RP2"
			fi
			;;
		"Linux")
			if [ -d "/media/$USER/RPI-RP2" ]; then
				umount "/media/$USER/RPI-RP2"
			fi
			;;
		"MINGW"*|"MSYS"*|"CYGWIN"*)  # Windows
			# Windows typically doesn't need explicit unmounting
			;;
	esac

	# Now reboot the Pico
	picotool reboot

	# Wait a moment for the device to reboot and reconnect
	sleep 2

else
	# Platform-specific installation instructions
	case $OS in
		"Darwin")
			echo "picotool not found! To install on macOS:"
			echo "brew install picotool"
			;;
		"Linux")
			echo "picotool not found! To install on Ubuntu/Debian:"
			echo "sudo apt install picotool"
			;;
		"MINGW"*|"MSYS"*|"CYGWIN"*)
			echo "picotool not found! To install on Windows:"
			echo "1. Install picotool through MSYS2 or"
			echo "2. Build from source: https://github.com/raspberrypi/picotool"
			;;
	esac
	echo "Or manually copy gamecube_controller_reader.uf2 to the Pico in BOOTSEL mode"
fi