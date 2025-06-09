#!/bin/bash
set -e

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

# Clean and recreate build directory
rm -rf "$SCRIPT_DIR/build"
mkdir "$SCRIPT_DIR/build"
cd "$SCRIPT_DIR/build"

# Build
cmake ..
make -j4

echo "Build complete!"