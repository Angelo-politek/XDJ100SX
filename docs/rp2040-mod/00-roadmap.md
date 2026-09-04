# XDJ100SX — RP2040 fork roadmap

This fork keeps the original CDJ-100S mainboard intact: instead of
desoldering each switch and wiring it point-to-point to the microcontroller
(as the upstream `/arduino` Teensy LC build does), the RP2040 taps the
existing front-panel connector and decodes the button/encoder matrix from
there. Nothing on the original board is cut or permanently modified.

## Phase 0 — Repo & branch setup ✅
- `rp2040-mod` branch created off `main`.
- `firmware/rp2040/` — Pico SDK project (replaces `/arduino` for this fork).
- `docs/rp2040-mod/` — planning and hardware research notes (this folder).

## Phase 1 — Raspberry Pi + display bring-up
- Flash Raspberry Pi OS (or reuse/adapt the upstream image) on the Pi 3B+.
- Install/build Mixxx, deploy the `/mixxx/SKIN` and `/mixxx/MIDI` assets.
- Bring up the main display and confirm the Mixxx GUI renders correctly on it.
- Re-apply the shutdown/eject/usb-share scripts from `/SCRIPTS` as needed.

## Phase 2 — RP2040 toolchain bring-up
- Confirm `firmware/rp2040` builds and flashes (LED blink bring-up firmware).
- Get a minimal USB-MIDI TinyUSB device enumerating on the Pi, visible in a
  MIDI monitor / Mixxx MIDI device list.

## Phase 3 — Non-destructive matrix decode research
- Identify the front-panel connector pinout without desoldering anything.
- See [`01-hardware-notes.md`](01-hardware-notes.md) for what the CDJ-100S
  service manual already tells us, and the probing plan for what it doesn't.

## Phase 4 — RP2040 input firmware
- Port the button/encoder/pitch-fader logic from `/arduino/XDJ100SX.ino` to
  the RP2040, adapted for a scanned matrix (PIO-driven scan is the likely
  approach) instead of one GPIO per switch.
- Re-implement the LED feedback (Play/Cue/Intern/CD) driven by MIDI note
  in/out, same protocol as upstream so `/mixxx/MIDI` mapping keeps working.

## Phase 5 — MIDI mapping & Mixxx integration
- Validate `/mixxx/MIDI/XDJ100SX.js` + `.midi.xml` against the RP2040 firmware.
- Adjust only where the new firmware's note/CC numbers diverge from upstream.

## Phase 6 — Base result
Feature parity with the upstream project (all original buttons, jog wheel,
browse encoder, pitch fader, LEDs working through Mixxx), but running on the
RP2040 + non-destructive matrix tap instead of the Teensy LC + rewired
switches.

## Phase 7+ — Expansions
Open — to be defined once the base result is working and stable. Candidate
ideas get their own doc under `docs/rp2040-mod/expansions/` as they're
scoped out, one file per feature.
