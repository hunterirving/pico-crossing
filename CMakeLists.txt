
cmake_minimum_required(VERSION 3.13)
include(pico_sdk_import.cmake)
project(gamecube_controller_reader C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
pico_sdk_init()

add_executable(gamecube_controller_reader
	main.cpp
	joybus.cpp
	device.cpp
	simulatedController.cpp
	display.cpp
	keymap.cpp
	keybuffer.cpp
	townTunes.cpp
	nookCodes.cpp
	design.cpp
	snake.cpp
)

pico_enable_stdio_usb(gamecube_controller_reader 1)
pico_enable_stdio_uart(gamecube_controller_reader 0)

pico_add_extra_outputs(gamecube_controller_reader)

# Generate both PIO headers
pico_generate_pio_header(gamecube_controller_reader ${CMAKE_CURRENT_LIST_DIR}/controller.pio)
pico_generate_pio_header(gamecube_controller_reader ${CMAKE_CURRENT_LIST_DIR}/joybus.pio)

target_link_libraries(gamecube_controller_reader 
	pico_stdlib 
	hardware_pio 
	pico_multicore
	pico_platform
)
