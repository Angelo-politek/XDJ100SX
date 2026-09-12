cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# The block below is what the "Raspberry Pi Pico" VS Code extension looks for to
# recognise this folder as a Pico project (together with a .vscode/ directory in
# the same folder, which the extension's "Import Pico Project" command creates).
# The extension rewrites the version numbers itself when you import the project
# or switch SDK, so leave the markers alone. Outside VS Code the include is
# simply skipped and the build falls back to PICO_SDK_PATH as usual.

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.1.1)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.1.1)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================

set(PICO_BOARD pico CACHE STRING "Board type")

include(pico_sdk_import.cmake)

project(xdj100sx_rp2040 C CXX ASM)

pico_sdk_init()

add_executable(xdj100sx_rp2040
    src/main.c
    src/matrix.c
    src/encoder.c
    src/leds.c
    src/pitch.c
    src/midi.c
    src/usb_dev.c
    src/usb_descriptors.c
)

target_include_directories(xdj100sx_rp2040 PRIVATE src)
target_compile_options(xdj100sx_rp2040 PRIVATE -Wall -Wextra)

# The USB device is built here rather than by pico_stdio_usb: that library is
# CDC-only and owns both the descriptors and tusb_config.h, so it cannot be
# combined with a MIDI interface. src/usb_dev.c provides the stdio driver and
# the 1200-baud BOOTSEL reset that it would otherwise have given us.
pico_enable_stdio_usb(xdj100sx_rp2040 0)
pico_enable_stdio_uart(xdj100sx_rp2040 0)

target_link_libraries(xdj100sx_rp2040
    pico_stdlib
    pico_unique_id
    hardware_adc
    tinyusb_device
    tinyusb_board
)

# Pico W needs the CYW43 driver for the onboard LED / future Wi-Fi use.
if (PICO_BOARD STREQUAL "pico_w")
    target_link_libraries(xdj100sx_rp2040 pico_cyw43_arch_none)
endif()

pico_add_extra_outputs(xdj100sx_rp2040)
