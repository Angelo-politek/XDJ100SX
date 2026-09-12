# RP2040 firmware

Drives the CDJ-100S front panel from a Raspberry Pi Pico and presents it to
Mixxx as a USB MIDI controller. Built with the official Pico SDK rather than
the Arduino core, for direct control over the USB descriptors (a composite
MIDI + serial device) and over the timing of the panel's matrix scan.

Unlike the Teensy build it does **not** expect one GPIO per switch: the panel
already scans its 13 buttons as a 5 × 3 matrix, and this firmware drives that
matrix through the panel's original connector.

- Pinout, wiring and bring-up: [`docs/rp2040-mod/02-wiring.md`](../../docs/rp2040-mod/02-wiring.md)
- Why the circuit works this way: [`docs/rp2040-mod/01-hardware.md`](../../docs/rp2040-mod/01-hardware.md)
- MIDI protocol: [`docs/rp2040-mod/03-midi.md`](../../docs/rp2040-mod/03-midi.md)

## Modules

| File | Role |
|---|---|
| `src/board_config.h` | the pin map and calibration — the only file you should need to edit |
| `src/matrix.c/h` | 5 × 3 scan, debounce, ghost rejection |
| `src/encoder.c/h` | interrupt-driven quadrature for jog and browse |
| `src/pitch.c/h` | oversampled ADC, adaptive filter, calibration |
| `src/leds.c/h` | on/off, blink, pulse |
| `src/midi.c/h` | MIDI protocol to Mixxx, LED notes back |
| `src/usb_dev.c/h` | composite USB device, stdio driver, BOOTSEL reset |
| `src/usb_descriptors.c` | USB descriptors |
| `src/main.c` | scheduling and the serial report |

## Build

Install the **Raspberry Pi Pico** VS Code extension and let it manage the SDK,
toolchain, CMake, Ninja and picotool.

Open **this folder** in VS Code, not the repository root: the extension only
activates on a workspace whose root holds `pico_sdk_import.cmake`. Then
*Raspberry Pi Pico: Import Pico Project* (once) and *Compile Project*.

From a shell, with `PICO_SDK_PATH` set:

```sh
cmake -B build -G Ninja -DPICO_BOARD=pico
cmake --build build
```

The result is `build/xdj100sx_rp2040.uf2`.

## Flash

Hold BOOTSEL while plugging the Pico in; it mounts as `RPI-RP2`, copy the
`.uf2` onto it. After the first flash the board can be reset into BOOTSEL by
opening its serial port at 1200 baud, so the extension's *Run* button — or
`picotool load -x`, even over SSH from the Raspberry Pi — works without
touching the hardware.

⚠️ Close the serial monitor before reflashing; it holds the port open and the
flash fails.

## Serial console

The firmware logs over USB CDC alongside MIDI, so the log stays readable while
Mixxx is connected.

| Key | Action |
|---|---|
| `h` | help |
| `r` | toggle the raw matrix dump, one line per row |
| `s` | one-shot status (pitch, encoders, held keys, MIDI state) |
| `z` | zero the encoders and the pitch min/max calibration |
| `p` | mute/unmute the pitch lines |
| `c` | hold one column high (S1..S5) to probe the strobe with a multimeter |
| `l` | LED self test |
| `1`-`4` | toggle one LED |
| `b` | simulate a 120 BPM beat flash, no host needed |
| `B` | pick which LED the beat flash uses |
