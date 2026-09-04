cmake_minimum_required(VERSION 3.13)

include(pico_sdk_import.cmake)

project(xdj100sx_rp2040 C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(xdj100sx_rp2040
    src/main.c
)

pico_enable_stdio_usb(xdj100sx_rp2040 0)
pico_enable_stdio_uart(xdj100sx_rp2040 1)

target_link_libraries(xdj100sx_rp2040
    pico_stdlib
)

# Pico W needs the CYW43 driver for the onboard LED / future Wi-Fi use.
if (PICO_BOARD STREQUAL "pico_w")
    target_link_libraries(xdj100sx_rp2040 pico_cyw43_arch_none)
endif()

pico_add_extra_outputs(xdj100sx_rp2040)
