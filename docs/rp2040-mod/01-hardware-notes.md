# Hardware notes — non-destructive front panel tap

Goal: read the CDJ-100S front-panel buttons/encoders from the RP2040 through
the existing connector(s), without cutting traces or desoldering switches.

## What the service manual already tells us

Source: `datasheets/pioneer_cdj-100s-sm-rrv2027.pdf`.

- Section 4 "PCB Connection Diagram" (p.24-29) shows four assemblies: Mother
  Board, Trans Board, SL Mecha Board, and **Display Board Assy** (p.28-29).
  The Display Board is the front panel: it carries the Play/Cue/Track/Search/
  Jet/Zip/Wah/Hold/Time/Master Tempo/Eject switches and the LEDs — this is
  the board this fork needs to intercept, not the CD mechanism or the Mother
  Board's digital audio section.
- The Display Board connects to the Mother Board via a single connector
  (silkscreened **CN801** on the Display Board / matching CN on the Mother
  Board, ribbon cable). Pin labels visible on the diagram around that
  connector look like key-matrix strobe/sense lines (`KO*` / `KI*` style
  naming), which would mean the switches are scanned as a row/column matrix
  by the Mother Board's CPU rather than each wired to its own line — this
  needs confirming once the connector is in hand and legible at full
  resolution (the scan in the repo is not sharp enough to read pin-by-pin).
- Section 8 "Panel Facilities and Specifications" (p.47) should list the
  full switch/LED inventory and may include the key matrix table directly —
  not yet reviewed in full, check this before probing.

## Probing plan (once the RP2040 and the disassembled unit are both available)

1. Re-read PDF pages 28-29 and 47 at full resolution / zoom before opening
   anything up — confirm whether it's really a scanned matrix and get the
   pin count.
2. Identify the connector type/pitch on the Display Board side (from the
   parts list, likely a standard FPC/FFC or pin-header matching `CN801`).
3. With the unit disassembled but the ribbon cable still connected between
   Display Board and Mother Board, use a multimeter in continuity mode to
   map each connector pin to its switch/LED, back-probing from known switch
   legs — no cutting needed for this step.
4. Cross-check against the service manual's schematic diagram (Section 3,
   p.10+, not yet reviewed) to confirm strobe (output) vs sense (input)
   lines and idle logic levels before connecting any RP2040 GPIO.
5. Build a small pass-through breakout (e.g. a 2x connector board or
   test clip) that taps the ribbon cable in parallel — RP2040 GPIOs read the
   same lines the Mother Board drives/reads, board stays fully original and
   still works standalone if the mod is removed.
6. Only after the matrix is understood and tapped safely: write the PIO/GPIO
   scan routine in `firmware/rp2040`.

## Open questions to resolve before wiring anything

- Is CN801 really a scanned matrix, or are some lines direct (e.g. the
  rotary encoders and pitch fader, which on the Teensy build are on their
  own dedicated pins already)?
- Idle/active logic levels and required pull resistors — must be confirmed
  before connecting RP2040 GPIOs, to avoid back-driving the Mother Board's
  own scan output.
- Connector keying/pitch, to choose the right tap hardware (test clip vs
  custom FFC breakout).
