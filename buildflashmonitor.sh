#!/bin/bash

echo "Starting build-flash-monitor sequence..."

# Run build script
echo "Building..."
./build.sh
if [ $? -ne 0 ]; then
	echo "Build failed! Aborting sequence."
	exit 1
fi

# Run flash script
echo "Flashing..."
./flash.sh
if [ $? -ne 0 ]; then
	echo "Flashing failed! Aborting sequence."
	exit 1
fi

# Run monitor script
echo "Monitoring..."
./monitor.sh