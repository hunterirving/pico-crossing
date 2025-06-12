#!/usr/bin/env python3
import os
import sys
import argparse
import time
from PIL import Image, ImageDraw
import numpy as np
from skimage import metrics
import json

# Define the 16 Animal Crossing palettes
PALETTES = [
	# palette 0
	["cd4a4a", "de8341", "e6ac18", "e6c520", "d5de18", "b4e618", "83d552", "39c56a", "29acc5", "417bee", "6a4ae6", "945acd", "bd41b4", "000000", "ffffff"],
	# palette 1
	["ff8b8b", "ffcd83", "ffe65a", "fff662", "ffff83", "deff52", "b4ff83", "7bf6ac", "62e616", "83c5ff", "a49cff", "d59cff", "ff9cf6", "8b8b8b", "ffffff"],
	# palette 2
	["9c1818", "ac5208", "b47b00", "b49400", "a4ac00", "83b400", "52a431", "089439", "007b94", "104abd", "3918ac", "5a2994", "8b087b", "080808", "ffffff"],
	# palette 3
	["41945a", "73c58b", "94e6ac", "008b7b", "5ab4ac", "83c5c5", "2073a4", "4a9ccd", "6aacde", "7383bd", "6a73ac", "525294", "39397b", "181862", "ffffff"],
	# palette 4
	["9c8352", "bd945a", "d4bc82", "9c5252", "cd7362", "ee9c8b", "8b6283", "a483b4", "deb4de", "bd8383", "ac736a", "945252", "7b3939", "621810", "ffffff"],
	# palette 5
	["ee5a00", "ff9c41", "ffcd83", "ffeea4", "8b4a29", "b47b5a", "e6ac8b", "ffdebd", "318bff", "62b4ff", "9cdeff", "c5e6ff", "6a6a6a", "000000", "ffffff"],
	# palette 6
	["39b441", "62de5a", "8bee83", "b4ffac", "2020c5", "5252f6", "8383ff", "b4b4ff", "cd3939", "de6a6a", "e68b9c", "eebdbd", "6a6a6a", "000000", "ffffff"],
	# palette 7
	["082000", "415a39", "6a8362", "9cb494", "5a2900", "7b4a20", "a4734a", "d5a47b", "947b00", "b49439", "cdb46a", "ded59c", "6a6a6a", "000000", "ffffff"],
	# palette 8
	["2020ff", "ff2020", "d5d500", "6262ff", "ff6262", "d5d562", "9494ff", "ff9494", "d5d594", "acacff", "ffacac", "e6e6ac", "6a6a6a", "000000", "ffffff"],
	# palette 9
	["20a420", "39acff", "9c52ee", "52bd52", "5ac5ff", "b49cff", "6ad573", "8be6ff", "cdb4ff", "93ddab", "bdf6ff", "d5cdff", "6a6a6a", "000000", "ffffff"],
	# palette 10
	["d50000", "ffbd00", "eef631", "4acd41", "299c29", "528bbd", "414aac", "9452d5", "f67bde", "a49439", "9c4141", "5a3139", "6a6a6a", "000000", "ffffff"],
	# palette 11
	["e6cd18", "20c518", "ff6a00", "0000ff", "9400bd", "e6cd18", "00a400", "cd4100", "0000d5", "5a008b", "9c8b18", "008300", "a42000", "0000a4", "4a005a"],
	# palette 12
	["ff2020", "e6d500", "f639bd", "00d59c", "107310", "c52020", "bda400", "cd3994", "009c6a", "204a20", "8b2020", "836a00", "941862", "00734a", "183918"],
	# palette 13
	["eed5d5", "dec5c5", "cdb4b4", "bda4a4", "ac9494", "9c8383", "8b7373", "7b6262", "6a5252", "5a4141", "4a3131", "392020", "291010", "180000", "100000"],
	# palette 14
	["eeeeee", "dedede", "cdcdcd", "bdbdbd", "acacac", "9c9c9c", "8b8b8b", "7b7b7b", "6a6a6a", "5a5a5a", "4a4a4a", "393939", "292929", "181818", "101010"],
	# palette 15
	["ee7b7b", "d51818", "f69418", "e6e652", "006a00", "39b439", "0039b4", "399cff", "940094", "ff6aff", "944108", "ee9c5a", "ffc594", "000000", "ffffff"]
]

# Convert hex colors to RGB tuples
RGB_PALETTES = []
for palette in PALETTES:
	rgb_palette = []
	for color in palette:
		r = int(color[0:2], 16)
		g = int(color[2:4], 16)
		b = int(color[4:6], 16)
		rgb_palette.append((r, g, b))
	RGB_PALETTES.append(rgb_palette)

def resize_image(image, target_size=32):
	"""Resize the image to have its smaller dimension equal to target_size,
	then crop from center to make it square with dimensions (target_size, target_size)"""
	
	# Get original dimensions
	width, height = image.size
	
	# Determine scale factor so the smaller dimension becomes target_size
	scale = target_size / min(width, height)
	
	# Calculate new dimensions
	new_width = int(width * scale)
	new_height = int(height * scale)
	
	# Resize image
	image = image.resize((new_width, new_height), Image.LANCZOS)
	
	# Calculate cropping box (center crop)
	left = (new_width - target_size) // 2
	top = (new_height - target_size) // 2
	right = left + target_size
	bottom = top + target_size
	
	# Crop to target_size x target_size
	image = image.crop((left, top, right, bottom))
	
	return image

def find_closest_color(color, palette):
	"""Find the closest color in the palette to the given color using Euclidean distance"""
	r, g, b = color
	min_distance = float('inf')
	closest_index = 0
	
	for i, (pr, pg, pb) in enumerate(palette):
		# Calculate Euclidean distance in RGB space - use int casting to avoid overflow
		r_diff = int(r) - int(pr)
		g_diff = int(g) - int(pg)
		b_diff = int(b) - int(pb)
		distance = float(r_diff * r_diff + g_diff * g_diff + b_diff * b_diff)
		
		if distance < min_distance:
			min_distance = distance
			closest_index = i
			
	return closest_index

def apply_floyd_steinberg_dithering(image, palette):
	"""Apply Floyd-Steinberg dithering to an image using a specific palette"""
	# Convert image to RGB mode if it's not already
	image = image.convert('RGB')
	
	# Create a copy to work with
	result = image.copy()
	width, height = result.size
	
	# Create numpy array from image for faster processing (use float for better precision)
	pixels = np.array(result, dtype=np.float32)
	
	# Apply Floyd-Steinberg dithering
	for y in range(height):
		for x in range(width):
			old_pixel = pixels[y, x].copy()
			
			# Find closest color in palette
			closest_idx = find_closest_color(old_pixel, palette)
			new_pixel = np.array(palette[closest_idx], dtype=np.float32)
			
			# Replace current pixel with the closest palette color
			pixels[y, x] = new_pixel
			
			# Compute quantization error
			quant_error = old_pixel - new_pixel
			
			# Distribute error to neighboring pixels with slightly enhanced coefficients
			# for more visible dithering in small images
			if x + 1 < width:
				pixels[y, x + 1] = np.clip(pixels[y, x + 1] + quant_error * 7 / 16, 0, 255)
			
			if y + 1 < height:
				if x - 1 >= 0:
					pixels[y + 1, x - 1] = np.clip(pixels[y + 1, x - 1] + quant_error * 3 / 16, 0, 255)
				
				pixels[y + 1, x] = np.clip(pixels[y + 1, x] + quant_error * 5 / 16, 0, 255)
				
				if x + 1 < width:
					pixels[y + 1, x + 1] = np.clip(pixels[y + 1, x + 1] + quant_error * 1 / 16, 0, 255)
	
	# Convert back to PIL Image and ensure it has the same size as the input
	result_image = Image.fromarray(pixels.astype(np.uint8))
	if result_image.size != image.size:
		result_image = result_image.resize(image.size, Image.LANCZOS)
		
	return result_image

def map_colors_without_dithering(image, palette):
	"""Map each pixel to the closest color in the palette without dithering"""
	# Convert image to RGB mode if it's not already
	image = image.convert('RGB')
	
	# Create a copy to work with
	result = image.copy()
	width, height = result.size
	
	# Create numpy array for output with the exact same dimensions
	output = np.zeros((height, width, 3), dtype=np.uint8)
	
	# For each pixel, find the closest color in the palette
	for y in range(height):
		for x in range(width):
			r, g, b = result.getpixel((x, y))
			closest_idx = find_closest_color((r, g, b), palette)
			output[y, x] = palette[closest_idx]
	
	# Convert back to PIL Image and ensure it has the same size as the input
	result_image = Image.fromarray(output)
	if result_image.size != image.size:
		result_image = result_image.resize(image.size, Image.LANCZOS)
		
	return result_image

def calculate_perceptual_error(original, processed):
	"""Calculate perceptual error between original and processed images using SSIM and color difference"""
	# Ensure both images are in RGB mode
	original = original.convert('RGB')
	processed = processed.convert('RGB')
	
	# Ensure both images have the same dimensions
	if original.size != processed.size:
		processed = processed.resize(original.size, Image.LANCZOS)
	
	# Convert images to numpy arrays
	original_array = np.array(original)
	processed_array = np.array(processed)
	
	# Calculate structural similarity index (higher is better)
	ssim_score = metrics.structural_similarity(
		original_array, processed_array, 
		multichannel=True, channel_axis=2,
		data_range=255
	)
	
	# Calculate mean color error (lower is better)
	color_diff = np.mean(np.abs(original_array.astype(float) - processed_array.astype(float)))
	color_error = color_diff / 255.0  # Normalize to [0, 1]
	
	# Calculate combined error score with emphasis on SSIM
	# Weight SSIM more heavily as it's better for perceptual quality
	error_score = (1.0 - ssim_score) * 0.7 + color_error * 0.3
	
	return error_score

def select_best_palette(image, use_dithering=True):
	"""Select the best palette for the image based on perceptual error"""
	best_palette_idx = 0
	best_error = float('inf')
	best_result = None
	
	# Try each palette
	for idx, palette in enumerate(RGB_PALETTES):
		# Process image with current palette
		if use_dithering:
			processed = apply_floyd_steinberg_dithering(image, palette)
		else:
			processed = map_colors_without_dithering(image, palette)
		
		# Calculate error
		error = calculate_perceptual_error(image, processed)
		
		# Update best if this palette has lower error
		if error < best_error:
			best_error = error
			best_palette_idx = idx
			best_result = processed
	
	return best_palette_idx, best_result

def create_color_indexes(processed_image, palette):
	"""Create a 2D array of color indexes (0-14) for the processed image"""
	width, height = processed_image.size
	color_indexes = np.zeros((height, width), dtype=np.uint8)
	
	for y in range(height):
		for x in range(width):
			r, g, b = processed_image.getpixel((x, y))
			color_indexes[y, x] = find_closest_color((r, g, b), palette)
	
	return color_indexes

def create_gif(processed_image, palette_idx, output_path):
	"""Create a GIF file with the processed image using the full color table"""
	# Create a new palette image with all colors from all palettes
	all_colors = []
	for palette in RGB_PALETTES:
		all_colors.extend(palette)
	
	# Create a palette image
	palette_img = Image.new('P', (1, 1))
	flat_palette = [component for color in all_colors for component in color]
	palette_img.putpalette(flat_palette)
	
	# Convert processed image to use the palette
	processed_image = processed_image.quantize(palette=palette_img)
	
	# Save as GIF
	processed_image.save(output_path, format='GIF')

def create_single_frame_frameset_code(color_indexes, palette_idx, input_filename=None):
	"""Create single-frame frameset implementation code using streaming structure"""
	timestamp = time.strftime("%Y-%m-%d %H:%M:%S")
	source_info = f"\t// Source file: {input_filename}" if input_filename else ""
	
	# Always use streaming mode for consistency, even for single images
	return create_streaming_frameset_code([(palette_idx, color_indexes)], input_filename, timestamp, source_info)

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
	
	code = f"""// Initialize with a generated streaming frameset from an image
\t// Generated at: {timestamp}
{source_info}
\t// Frame count: {frame_count} frame{'s' if frame_count != 1 else ''} (using streaming mode)
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

def update_design_hpp(color_indexes, palette_idx, design_hpp_path, input_filename=None):
	"""Update the design.hpp file with the frameset"""
	import re
	
	# Generate the new frameset implementation code
	new_frameset_code = create_single_frame_frameset_code(color_indexes, palette_idx, input_filename)
	
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

def ensure_output_dir_exists(script_dir):
	"""Ensure the preview_gifs directory exists"""
	output_dir = os.path.join(script_dir, "preview_gifs")
	if not os.path.exists(output_dir):
		os.makedirs(output_dir)
	return output_dir

def main():
	parser = argparse.ArgumentParser(description='Convert an image to an Animal Crossing frameset.')
	parser.add_argument('input_image', help='Path to the input image file')
	parser.add_argument('-o', '--output', help='Output base name (without extension)', default='output')
	parser.add_argument('--nodither', action='store_true', default=False, 
						help='Disable dithering (default: dithering is enabled)')
	parser.add_argument('-p', '--palette', type=int, choices=range(16), metavar='{0-15}',
						help='Force a specific palette (0-15) for the image instead of auto-selecting')
	parser.add_argument('--project-dir', help='Path to the project directory', 
						default=os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
	
	args = parser.parse_args()
	
	try:
		# Get the directory where the script is located
		script_dir = os.path.dirname(os.path.abspath(__file__))
		
		# Ensure preview_gifs directory exists
		output_dir = ensure_output_dir_exists(script_dir)
		
		# Get the input filename for the comments
		input_filename = os.path.basename(args.input_image)
		
		# Load and process the image
		try:
			original_image = Image.open(args.input_image)
		except Exception as e:
			print(f"Error opening image: {e}")
			sys.exit(1)
		
		# Resize the image to 32x32
		resized_image = resize_image(original_image)
		
		# Ensure image is in RGB mode
		resized_image = resized_image.convert('RGB')
		
		# Use specified palette or select the best palette and process the image
		use_dithering = not args.nodither
		if args.palette is not None:
			# Use the specified palette
			palette_idx = args.palette
			if use_dithering:
				processed_image = apply_floyd_steinberg_dithering(resized_image, RGB_PALETTES[palette_idx])
			else:
				processed_image = map_colors_without_dithering(resized_image, RGB_PALETTES[palette_idx])
		else:
			# Auto-select the best palette
			palette_idx, processed_image = select_best_palette(resized_image, use_dithering)
		
		# Ensure processed image is the correct size (32x32)
		if processed_image.size != (32, 32):
			processed_image = processed_image.resize((32, 32), Image.LANCZOS)
		
		# Create color indexes
		color_indexes = create_color_indexes(processed_image, RGB_PALETTES[palette_idx])
		
		# Create output paths - always save to preview_gifs directory
		base_name = args.output
		output_filename = f"{base_name}.gif"
		gif_output = os.path.join(output_dir, output_filename)
		
		# Path to design.hpp
		design_hpp_path = os.path.join(args.project_dir, "src", "design.hpp")
		
		# Create the GIF for visual reference
		create_gif(processed_image, palette_idx, gif_output)
		
		# Update design.hpp with the generated frameset, including original filename
		update_design_hpp(color_indexes, palette_idx, design_hpp_path, input_filename)
		
		print(f"Processing complete!")
		if args.palette is not None:
			print(f"Using specified palette: {palette_idx}")
		else:
			print(f"Auto-selected palette: {palette_idx}")
		print(f"Dithering: {'Disabled' if args.nodither else 'Enabled'}")
		print(f"Output GIF: {gif_output}")
		print(f"Frameset successfully integrated into design.hpp")
	
	except Exception as e:
		print(f"Error processing image: {str(e)}")
		import traceback
		traceback.print_exc()
		sys.exit(1)

if __name__ == "__main__":
	main()