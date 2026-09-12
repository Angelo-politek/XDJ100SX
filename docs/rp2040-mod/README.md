# RP2040 front-panel firmware

An alternative to the Teensy LC build that **leaves the CDJ-100S front panel
intact**.

The upstream build desolders every switch, cuts the shared ground traces and
wires each button point-to-point. This one does not need to: the panel already
scans its 13 buttons as a 5 × 3 matrix, and that matrix — along with the jog
encoder and the pitch fader — comes out on a single 36-pin connector. With the
Mother Board removed those lines are free, so the RP2040 takes the original
CPU's place and drives the matrix exactly as it did.

| | Teensy LC build | this one |
|---|---|---|
| Controller | Teensy LC | Raspberry Pi Pico (RP2040) |
| Toolchain | Arduino IDE | Pico SDK, C, VS Code |
| Buttons | 13 GPIOs, one per switch | 5 × 3 matrix on the original connector |
| Traces cut | yes | **none** |
| Switches desoldered | all | **none** |
| Board modifications | extensive | 2, both reversible |

The two modifications are four resistors added in parallel for LED brightness,
and one lifted LED leg — the single indicator Pioneer wired permanently on.
Everything else is signal tapping.

The MIDI note and CC numbers are unchanged, and the USB device announces itself
with the same name as the Teensy, so `mixxx/MIDI/` works as-is and the mapping
in the project image comes up **already selected and enabled**.

## Documentation

| File | Contents |
|---|---|
| [`01-hardware.md`](01-hardware.md) | How the panel works: key matrix, LED drivers, why 3.3 V. Useful for the Teensy build too. |
| [`02-wiring.md`](02-wiring.md) | Build sheet: CN601 pinout, wire list, modifications, bring-up, diagnostics |
| [`03-midi.md`](03-midi.md) | MIDI protocol and the USB implementation |

Firmware: [`firmware/rp2040/`](../../firmware/rp2040/).

## Status

Working on a real deck: all 13 buttons, jog, pitch fader and all four LEDs,
driving Mixxx on the project's Raspberry Pi 3B+ image.

Still open: the browse encoder (three wires to spare GPIOs, external to the
panel) is wired in firmware but has not been tested against hardware yet.

## Hardware needed

Beyond the base project: a Raspberry Pi Pico, about 25 lengths of AWG 30 wire,
four 150 Ω resistors, a fine-tipped iron, flux and a multimeter with a
continuity beeper.
