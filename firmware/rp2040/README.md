# XDJ100SX — RP2040 firmware (Pico SDK)

Replaces the Teensy LC firmware (`/arduino`) for this fork. Built with the
official Raspberry Pi Pico C/C++ SDK instead of the Arduino core, to get
direct control over USB descriptors (custom USB-MIDI) and PIO (button matrix
scanning) once the hardware side of the project is defined.

Current state: bring-up only. `src/main.c` blinks the onboard LED so the
toolchain and flashing workflow can be verified as soon as the board arrives.
No XDJ100SX-specific logic (matrix scan, encoders, pitch fader, USB MIDI)
exists yet — see [`/docs/rp2040-mod/00-roadmap.md`](../../docs/rp2040-mod/00-roadmap.md)
for the planned build-out order.

## Prerequisites

- [pico-sdk](https://github.com/raspberrypi/pico-sdk) cloned locally (with submodules: `git submodule update --init`)
- CMake >= 3.13
- `arm-none-eabi-gcc` toolchain (e.g. via the ARM GNU Toolchain installer, or `choco install gcc-arm-embedded` on Windows)
- Ninja or GNU Make

## Build

Set `PICO_SDK_PATH` to point at your local pico-sdk checkout, then:

```sh
cd firmware/rp2040
mkdir build && cd build
cmake -G Ninja -DPICO_BOARD=pico ..   # or -DPICO_BOARD=pico_w
ninja
```

This produces `xdj100sx_rp2040.uf2` in `build/`.

## Flash

Hold **BOOTSEL** while plugging the Pico into USB, it will mount as a mass
storage device (`RPI-RP2`). Copy `xdj100sx_rp2040.uf2` onto it; the board
reboots and runs the new firmware automatically.
