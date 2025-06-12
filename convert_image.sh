#!/bin/bash

# Check for input file argument
if [ $# -lt 1 ]; then
    echo "Usage: $0 <input_image> [output_name] [--nodither] [-p=<value>|--palette=<value>]"
    echo ""
    echo "Options:"
    echo "  <input_image>         Path to the input image file (required)"
    echo "  [output_name]         Base name for output files (default: same as input without extension)"
    echo "  [--nodither]          Disable dithering (default: dithering enabled)"
    echo "  [-p=<value>|--palette=<value>] Force a specific palette (0-15) for the image instead of auto-selecting"
    exit 1
fi

# Set input file and handle output name
INPUT_FILE="$1"
shift

# Get file name without extension as default output name
FILENAME=$(basename -- "$INPUT_FILE")
OUTPUT_NAME="${FILENAME%.*}"

# Default values
NODITHER=""
PALETTE=""

# Parse the remaining arguments
while [ $# -gt 0 ]; do
    case "$1" in
        --nodither)
            NODITHER="--nodither"
            ;;
        -p=*)
            PALETTE="--palette=${1#*=}"
            ;;
        --palette=*)
            PALETTE="--palette=${1#*=}"
            ;;
        -p)
            shift
            if [ $# -gt 0 ]; then
                PALETTE="--palette $1"
            else
                echo "Error: -p option requires a palette number (0-15)"
                exit 1
            fi
            ;;
        *)
            # Assume it's the output name if it doesn't start with --
            if [[ ! "$1" == --* && ! "$1" == -* ]]; then
                OUTPUT_NAME="$1"
            else
                echo "Unknown option: $1"
                exit 1
            fi
            ;;
    esac
    shift
done

# Set up script directory and tool directory
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
TOOL_DIR="$SCRIPT_DIR/image_tools"
VENV_DIR="$TOOL_DIR/env"

# Check if Python 3 is installed
if ! command -v python3 &> /dev/null; then
    echo "Error: Python 3 is required but not found."
    echo "Please install Python 3 and try again."
    exit 1
fi

# Check if virtual environment exists, create if it doesn't
if [ ! -d "$VENV_DIR" ]; then
    echo "Setting up virtual environment for the first time..."
    # Create virtual environment
    python3 -m venv "$VENV_DIR" || {
        echo "Error: Failed to create virtual environment."
        echo "Please make sure 'python3-venv' package is installed."
        exit 1
    }
    
    # Activate and install dependencies
    source "$VENV_DIR/bin/activate"
    echo "Installing required packages..."
    pip install --upgrade pip
    pip install pillow numpy scikit-image || {
        echo "Error: Failed to install required packages."
        deactivate
        exit 1
    }
    echo "Setup complete!"
else
    # Activate existing environment
    source "$VENV_DIR/bin/activate"
fi

# Run the conversion tool
echo "Converting $INPUT_FILE to frameset..."
python "$TOOL_DIR/image_to_frameset.py" "$INPUT_FILE" -o "$OUTPUT_NAME" --project-dir "$SCRIPT_DIR" $NODITHER $PALETTE

# Check if conversion was successful
if [ $? -eq 0 ]; then
    echo ""
    echo "Success! The single-frame frameset has been integrated into design.hpp."
    echo ""
    echo "Build and flash the project to use the new design:"
    echo "   ./buildflashmonitor.sh"
fi

# Deactivate virtual environment
deactivate