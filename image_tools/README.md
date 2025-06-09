# Image and Video Tools

This directory contains tools for converting images and videos into framesets that can be displayed in Animal Crossing's design editor.

## Tools

### Image Converter
Converts a single image into a frameset (one frame).

### Video Converter
Converts a video into an animated frameset (multiple frames).

## Requirements

- Python 3.6+ (with venv module)
- ffmpeg (for video conversion only)

All required Python packages will be automatically installed in a virtual environment by the scripts:
- Pillow (PIL)
- NumPy
- scikit-image

## Quick Start

Use the provided shell scripts from the project root directory:

### Image Conversion
```bash
./convert_image.sh input_image.jpg [output_name] [options]
```

**Options:**
- `--nodither` - Disable dithering for sharper but less detailed results
- `--palette=N` - Force a specific palette (0-15) instead of auto-selection

**Examples:**
```bash
# Basic usage
./convert_image.sh monalisa.jpg

# Custom name with specific palette
./convert_image.sh my_image.png custom_design --palette=5 --nodither
```

### Video Conversion
```bash
./convert_video.sh input_video.mp4 [output_name] [options]
```

**Options:**
- `--fps=N` - Target frames per second (lower = faster conversion)
- `--max-frames=N` - Limit total number of frames
- `--start-frame=N` - Start from specific frame in source video
- `--stop-frame=N` - Stop at specific frame in source video
- `--separate-frames` - Create individual frame previews instead of animated GIF
- `--nodither` - Disable dithering
- `--palette=N` - Force specific palette for all frames

**Examples:**
```bash
# Basic usage
./convert_video.sh rickroll.mp4

# Advanced options
./convert_video.sh video.mp4 animation --fps=5 --max-frames=50 --start-frame=30
```

## Output Files

The tools produce:

1. **Preview GIF(s)**: Visual representation of how the content will appear in-game
   - Single animated GIF for videos (default)
   - Individual frame GIFs when using `--separate-frames`
2. **Automatic Integration**: Updates `design.hpp` directly with the generated frameset code

## How It Works

### Image Processing
1. Resizes image to 32x32 pixels
2. Tests all 16 available color palettes to find the best match
3. Applies dithering (if enabled) to improve color representation
4. Generates preview GIF using the selected palette
5. Creates C++ frameset code

### Video Processing
1. Extracts frames from video using ffmpeg
2. Processes each frame like an image
3. Optimizes palette selection across all frames
4. Creates animated preview showing the sequence
5. Generates multi-frame C++ frameset code

## Integration

The tools automatically integrate framesets into your project:

1. Frameset code is written directly into `design.hpp`
2. Rebuild and flash to load the generated frameset onto your Pi: `./buildflashmonitor.sh`

## File Structure

- `image_to_frameset.py` - Core image conversion logic
- `video_to_frameset.py` - Core video conversion logic
- `env/` - Python virtual environment (auto-created)
- `preview_gifs/` - Generated preview files
- `test_images/` - Sample images and videos for testing