#!/usr/bin/env python3
import os
import sys
import argparse
import time
import tempfile
import subprocess
from PIL import Image
import numpy as np
from skimage import metrics
import json
import re
import shutil

# Import functions from the image_to_frameset module
sys.path.append(os.path.dirname(os.path.abspath(__file__)))
from image_to_frameset import (
    RGB_PALETTES, resize_image, apply_floyd_steinberg_dithering,
    map_colors_without_dithering, select_best_palette, create_color_indexes,
    create_gif, calculate_perceptual_error, find_closest_color, ensure_output_dir_exists
)

def check_ffmpeg():
    """Verify that ffmpeg is installed and available"""
    try:
        result = subprocess.run(
            ["ffmpeg", "-version"], 
            stdout=subprocess.PIPE, 
            stderr=subprocess.PIPE
        )
        return result.returncode == 0
    except FileNotFoundError:
        return False

def extract_video_frames(video_path, output_dir, fps=None, max_frames=None, start_frame=None, stop_frame=None):
    """Extract frames from a video using ffmpeg"""
    if not os.path.exists(output_dir):
        os.makedirs(output_dir)

    # Get original video FPS for start/stop frame calculation
    original_fps = None
    if start_frame is not None or stop_frame is not None:
        # Get the original video FPS
        probe_cmd = ["ffprobe", "-v", "quiet", "-show_entries", "stream=r_frame_rate", 
                    "-select_streams", "v:0", "-of", "csv=p=0", video_path]
        try:
            result = subprocess.run(probe_cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
            if result.returncode == 0:
                fps_str = result.stdout.strip()
                # Handle fractional frame rates like "30000/1001"
                if '/' in fps_str:
                    num, den = fps_str.split('/')
                    original_fps = float(num) / float(den)
                else:
                    original_fps = float(fps_str)
        except:
            print("Warning: Could not determine original video FPS, using frame selection filter")

    # Calculate max_frames from stop_frame if provided
    # Note: When using fps conversion, max_frames represents the target output frame count
    if stop_frame is not None and start_frame is not None:
        if fps and original_fps:
            # Calculate how many frames we need at the target FPS
            duration_in_original_frames = stop_frame - start_frame
            duration_in_seconds = duration_in_original_frames / original_fps
            calculated_max_frames = int(duration_in_seconds * fps)
        else:
            # No FPS conversion, use frame count directly
            calculated_max_frames = stop_frame - start_frame
        
        if max_frames is not None:
            print(f"Warning: Both --max-frames ({max_frames}) and --stop-frame ({stop_frame}) specified. Using --stop-frame.")
        max_frames = calculated_max_frames
    elif stop_frame is not None and start_frame is None:
        if fps and original_fps:
            # Calculate frames needed from start to stop_frame at target FPS
            duration_in_seconds = stop_frame / original_fps
            max_frames = int(duration_in_seconds * fps)
        else:
            max_frames = stop_frame

    # Extract frames at original resolution initially - we'll resize with PIL later
    # This approach is more reliable than complex ffmpeg filters across different versions
    cmd = ["ffmpeg"]
    
    # Add start frame seek if specified
    if start_frame is not None and original_fps:
        # Use time-based seeking with original video FPS
        start_time = start_frame / original_fps
        cmd.extend(["-ss", str(start_time)])
    
    cmd.extend(["-i", video_path])
    
    # Use frame selection filter if start_frame is specified but no original fps
    if start_frame is not None and not original_fps:
        cmd.extend(["-vf", f"select='gte(n,{start_frame})'"])
    
    # Add FPS limitation if specified
    if fps:
        cmd.extend(["-r", str(fps)])
    
    # Add frame limit if specified
    if max_frames:
        cmd.extend(["-vframes", str(max_frames)])
    
    # Output pattern
    cmd.append(os.path.join(output_dir, "frame_%04d.png"))
    
    # Run ffmpeg with verbose output for debugging
    print(f"Running ffmpeg command: {' '.join(cmd)}")
    result = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    
    if result.returncode != 0:
        print(f"FFmpeg stderr: {result.stderr.decode()}")
        raise Exception(f"Error extracting frames from video: {result.stderr.decode()}")
    
    # Debug: Show what files were actually created
    if os.path.exists(output_dir):
        created_files = [f for f in os.listdir(output_dir) if f.startswith("frame_") and f.endswith(".png")]
        print(f"FFmpeg created {len(created_files)} frame files: {sorted(created_files)}")

def process_frame(frame_path, use_dithering=True, palette_idx=None):
    """Process a single video frame using the image processing pipeline"""
    # Load the frame
    frame = Image.open(frame_path)
    
    # Ensure the frame is properly sized using the same method as for images
    # This adds an extra safeguard in case ffmpeg scaling isn't perfect
    frame = resize_image(frame, target_size=32)
    
    if palette_idx is not None:
        # Use the specified palette
        if use_dithering:
            processed_frame = apply_floyd_steinberg_dithering(frame, RGB_PALETTES[palette_idx])
        else:
            processed_frame = map_colors_without_dithering(frame, RGB_PALETTES[palette_idx])
    else:
        # Select best palette and process image
        palette_idx, processed_frame = select_best_palette(frame, use_dithering)
    
    # Create color indexes
    color_indexes = create_color_indexes(processed_frame, RGB_PALETTES[palette_idx])
    
    return palette_idx, color_indexes, processed_frame

def create_multi_frame_frameset_code(frames_data, input_filename=None):
    """Create multi-frame frameset implementation code"""
    timestamp = time.strftime("%Y-%m-%d %H:%M:%S")
    source_info = f"\t// Source file: {input_filename}" if input_filename else ""
    
    # Always use streaming mode for videos
    return create_streaming_frameset_code(frames_data, input_filename, timestamp, source_info)

def create_streaming_frameset_code(frames_data, input_filename, timestamp, source_info):
    """Create streaming frameset implementation that stores frames in flash"""
    frame_count = len(frames_data)
    
    # Generate frame data array (stored in flash)
    frame_data_entries = []
    for i, (palette_idx, color_indexes) in enumerate(frames_data):
        # Flatten the 32x32 pixel array to a single array
        pixel_bytes = []
        for row in color_indexes:
            pixel_bytes.extend(row)
        
        pixel_str = ", ".join(str(idx) for idx in pixel_bytes)
        frame_data_entries.append(f"\t\t{palette_idx}, {pixel_str}")
    
    frame_data_str = ",\n".join(frame_data_entries)
    
    code = f"""// Initialize with a generated streaming frameset from a video
\t// Generated at: {timestamp}
{source_info}
\t// Frame count: {frame_count} frames (using streaming mode)
\t
\t// Frame data stored in flash memory (format: paletteId followed by 1024 pixel bytes per frame)
\tstatic const uint8_t streamingFrameData[] = {{
{frame_data_str}
\t}};
\t
\tstatic StreamingFrameProvider streamingProvider(streamingFrameData, {frame_count});
\t
\tinline void initGeneratedFrameset() {{
\t\t// Get reference to the frameset
\t\tFrameset& fs = getCurrentFrameset();
\t\t
\t\t// Clear any existing frames and set up streaming provider
\t\tfs.frames.clear();
\t\tfs.provider = &streamingProvider;
\t\tfs.currentFrameIndex = 0;
\t}}"""
    
    return code


def update_design_hpp(frames_data, design_hpp_path, input_filename=None):
    """Update the design.hpp file with the multi-frame frameset"""
    # Generate the new frameset implementation code
    new_frameset_code = create_multi_frame_frameset_code(frames_data, input_filename)
    
    # Read the entire design.hpp file
    with open(design_hpp_path, 'r') as f:
        content = f.read()
    
    # Pattern to match the entire streaming frameset block from comment to the initGeneratedFrameset function
    # This captures everything including the streamingFrameData array, StreamingFrameProvider, and init function
    streaming_pattern = r'// Initialize with a generated streaming frameset.*?inline void initGeneratedFrameset\(\) \{[^}]*\}'
    
    # Check if there's already a streaming frameset implementation
    if re.search(streaming_pattern, content, re.DOTALL):
        # Replace the existing streaming implementation with the new one
        new_content = re.sub(streaming_pattern, new_frameset_code, content, flags=re.DOTALL)
    else:
        # Also check for any frameset from image (fallback pattern) - use same comprehensive pattern
        image_pattern = r'// Initialize with a generated streaming frameset.*?inline void initGeneratedFrameset\(\) \{[^}]*\}'
        if re.search(image_pattern, content, re.DOTALL):
            # Replace the existing image implementation with the new streaming one
            new_content = re.sub(image_pattern, new_frameset_code, content, flags=re.DOTALL)
        else:
            # No existing implementation found, add before the namespace closing brace
            namespace_end = content.rfind("}")
            if namespace_end > 0:
                # Insert before the namespace closing bracket
                new_content = content[:namespace_end] + "\n" + new_frameset_code + "\n" + content[namespace_end:]
            else:
                # Fallback - append to the end (shouldn't happen with a well-formed file)
                new_content = content + "\n" + new_frameset_code
    
    # Write the updated content back to design.hpp
    with open(design_hpp_path, 'w') as f:
        f.write(new_content)

def create_animated_gif(frames, palette_indices, output_path):
    """Create an animated GIF from multiple processed frames
    
    Args:
        frames: List of processed PIL Image objects
        palette_indices: List of palette indices used for each frame
        output_path: Path to save the animated GIF
    """
    print(f"Creating animated GIF with {len(frames)} input frames")
    
    # Create a list to hold the PIL Images for the animation
    gif_frames = []
    
    # Process each frame to use the palette
    for i, (frame, palette_idx) in enumerate(zip(frames, palette_indices)):
        print(f"Processing frame {i+1} for GIF (palette {palette_idx})")
        # Create a copy of the frame
        frame_copy = frame.copy()
        
        # Append to the list of frames
        gif_frames.append(frame_copy)
    
    print(f"Prepared {len(gif_frames)} frames for GIF creation")
    
    # Save as an animated GIF
    # The first frame determines all frames' palettes in PIL, which isn't ideal
    # but is a limitation we'll work with for preview purposes
    if gif_frames:
        # Force each frame to be slightly different to prevent PIL from dropping "duplicate" frames
        for i, frame in enumerate(gif_frames):
            # Add a tiny imperceptible difference to ensure frames aren't considered duplicates
            pixels = list(frame.getdata())
            if len(pixels) > 0:
                # Modify the last pixel by 1 in the red channel (imperceptible change)
                r, g, b = pixels[-1]
                # Add frame index to ensure uniqueness, but keep within valid range
                new_r = min(255, max(0, r + (i % 2)))  # Alternate between +0 and +1
                pixels[-1] = (new_r, g, b)
                frame.putdata(pixels)
        
        # Use a reasonable frame duration (100ms per frame)
        gif_frames[0].save(
            output_path,
            format='GIF',
            append_images=gif_frames[1:],
            save_all=True,
            duration=100,  # 100ms per frame
            loop=0,        # Loop forever
            optimize=False,  # Disable optimization to prevent frame dropping
            disposal=2      # Clear each frame before next one
        )
        
        # Verify the saved GIF
        from PIL import Image
        saved_gif = Image.open(output_path)
        frame_count = 0
        try:
            while True:
                saved_gif.seek(frame_count)
                frame_count += 1
        except EOFError:
            pass
        print(f"Saved GIF contains {frame_count} frames")
        
        return True
    return False

def main():
    parser = argparse.ArgumentParser(description='Convert a video to an Animal Crossing multi-frame frameset.')
    parser.add_argument('input_video', help='Path to the input video file')
    parser.add_argument('-o', '--output', help='Output base name (without extension)', default=None)
    parser.add_argument('--fps', type=float, help='Target frames per second (default: use original video fps)')
    parser.add_argument('--max-frames', type=int, help='Maximum number of frames to extract')
    parser.add_argument('--nodither', action='store_true', default=False, 
                        help='Disable dithering (default: dithering is enabled)')
    parser.add_argument('-p', '--palette', type=int, choices=range(16), metavar='{0-15}',
                        help='Force a specific palette (0-15) for all frames instead of auto-selecting')
    parser.add_argument('--separate-frames', action='store_true', default=False,
                        help='Create separate preview GIFs for each frame instead of a single animated GIF')
    parser.add_argument('--start-frame', type=int, help='Start processing from this frame number (0-based)')
    parser.add_argument('--stop-frame', type=int, help='Stop processing at this frame number (0-based, exclusive)')
    parser.add_argument('--project-dir', help='Path to the project directory', 
                        default=os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    
    args = parser.parse_args()
    
    try:
        # Check if ffmpeg is available
        if not check_ffmpeg():
            print("Error: ffmpeg is not installed or not in your PATH.")
            print("Please install ffmpeg and try again.")
            sys.exit(1)
        
        # Get the directory where the script is located
        script_dir = os.path.dirname(os.path.abspath(__file__))
        
        # Ensure preview_gifs directory exists
        output_dir = ensure_output_dir_exists(script_dir)
        
        # Get the input filename for the comments
        input_filename = os.path.basename(args.input_video)
        
        # Set default output name if not provided
        if args.output is None:
            # Get file name without extension as default output name
            args.output = os.path.splitext(input_filename)[0]
        
        # Create a temporary directory for frame extraction
        with tempfile.TemporaryDirectory() as temp_dir:
            print(f"Extracting frames from {args.input_video}...")
            extract_video_frames(
                args.input_video, 
                temp_dir,
                fps=args.fps,
                max_frames=args.max_frames,
                start_frame=args.start_frame,
                stop_frame=args.stop_frame
            )
            
            # Get a sorted list of all extracted frames
            frame_files = sorted([
                os.path.join(temp_dir, f) 
                for f in os.listdir(temp_dir) 
                if f.startswith("frame_") and f.endswith(".png")
            ])
            
            if not frame_files:
                print("Error: No frames were extracted from the video.")
                sys.exit(1)
            
            # Verify extracted frame count matches expected
            if args.max_frames and len(frame_files) != args.max_frames:
                print(f"Error: Expected {args.max_frames} frames but extracted {len(frame_files)} frames")
                print(f"Available frames: {[os.path.basename(f) for f in frame_files]}")
                print(f"This indicates the video may be shorter than expected or ffmpeg extraction failed")
                print(f"Try reducing --max-frames or check the video duration")
                sys.exit(1)
            
            print(f"Processing {len(frame_files)} frames...")
            
            # Process each frame
            frames_data = []  # List to store (palette_idx, color_indexes) tuples
            processed_frames = []  # List to store processed PIL Image objects
            palette_indices = []  # List to store palette indices
            use_dithering = not args.nodither
            
            for i, frame_file in enumerate(frame_files):
                print(f"Processing frame {i+1}/{len(frame_files)}...", end="\r")
                
                # Process the frame
                palette_idx, color_indexes, processed_frame = process_frame(
                    frame_file,
                    use_dithering=use_dithering,
                    palette_idx=args.palette
                )
                
                # If separate frames are requested, save individual GIFs
                if args.separate_frames:
                    gif_output = os.path.join(output_dir, f"{args.output}_frame_{i+1:04d}.gif")
                    create_gif(processed_frame, palette_idx, gif_output)
                
                # Store frame data for design.hpp and for animated GIF
                frames_data.append((palette_idx, color_indexes))
                processed_frames.append(processed_frame)
                palette_indices.append(palette_idx)
            
            print(f"\nAll frames processed successfully!")
            
            # Create animated GIF (if not using separate frames)
            if not args.separate_frames:
                animated_gif_path = os.path.join(output_dir, f"{args.output}_animated.gif")
                if create_animated_gif(processed_frames, palette_indices, animated_gif_path):
                    print(f"Created animated GIF: {animated_gif_path}")
            
            # Path to design.hpp
            design_hpp_path = os.path.join(args.project_dir, "design.hpp")
            
            # Update design.hpp with the generated frameset
            update_design_hpp(frames_data, design_hpp_path, input_filename)
            
            if args.palette is not None:
                print(f"Using specified palette for all frames: {args.palette}")
            else:
                print("Using auto-selected palette for each frame")
            print(f"Dithering: {'Disabled' if args.nodither else 'Enabled'}")
            if args.separate_frames:
                print(f"Output GIFs: {output_dir}/{args.output}_frame_*.gif")
            else:
                print(f"Output GIF: {output_dir}/{args.output}_animated.gif")
            print(f"Multi-frame frameset successfully integrated into design.hpp")
    
    except Exception as e:
        print(f"Error processing video: {str(e)}")
        import traceback
        traceback.print_exc()
        sys.exit(1)

if __name__ == "__main__":
    main()