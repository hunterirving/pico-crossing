#!/bin/bash

# Check for input file argument
if [ $# -lt 1 ]; then
    echo "Usage: $0 <input_video> [output_name] [--nodither] [--fps=<value>] [--max-frames=<value>] [--start-frame=<value>] [--stop-frame=<value>] [--separate-frames] [-p=<value>|--palette=<value>]"
    echo ""
    echo "Options:"
    echo "  <input_video>         Path to the input video file (required)"
    echo "  [output_name]         Base name for output files (default: same as input without extension)"
    echo "  [--nodither]          Disable dithering (default: dithering enabled)"
    echo "  [--fps=<value>]       Target frames per second (default: use original video fps)"
    echo "  [--max-frames=<value>] Maximum number of frames to extract"
    echo "  [--start-frame=<value>] Start processing from this frame number in the original video (0-based)"
    echo "  [--stop-frame=<value>] Stop processing at this frame number in the original video (0-based, exclusive)"
    echo "  [--separate-frames]   Create separate preview GIFs for each frame instead of a single animated GIF"
    echo "  [-p=<value>|--palette=<value>] Force a specific palette (0-15) for all frames instead of auto-selecting"
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
FPS=""
MAX_FRAMES=""
START_FRAME=""
STOP_FRAME=""
SEPARATE_FRAMES=""
PALETTE=""

# Parse the remaining arguments
while [ $# -gt 0 ]; do
    case "$1" in
        --nodither)
            NODITHER="--nodither"
            ;;
        --fps=*)
            FPS="--fps=${1#*=}"
            ;;
        --max-frames=*)
            MAX_FRAMES="--max-frames=${1#*=}"
            ;;
        --start-frame=*)
            START_FRAME="--start-frame=${1#*=}"
            ;;
        --stop-frame=*)
            STOP_FRAME="--stop-frame=${1#*=}"
            ;;
        --separate-frames)
            SEPARATE_FRAMES="--separate-frames"
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

# Check if ffmpeg is installed
if ! command -v ffmpeg &> /dev/null; then
    echo "Error: ffmpeg is required but not found."
    echo "Please install ffmpeg and try again."
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
python "$TOOL_DIR/video_to_frameset.py" "$INPUT_FILE" -o "$OUTPUT_NAME" $NODITHER $FPS $MAX_FRAMES $START_FRAME $STOP_FRAME $SEPARATE_FRAMES $PALETTE

# Check if conversion was successful
if [ $? -eq 0 ]; then
    echo ""
    echo "Success! The multi-frame frameset has been integrated into design.hpp."
    echo ""
    echo "Build and flash the project to use the new design:"
    echo "   ./buildflashmonitor.sh"
fi

# Deactivate virtual environment
deactivate