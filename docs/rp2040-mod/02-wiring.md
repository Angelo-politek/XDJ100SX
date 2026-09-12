# Wiring — CDJ-100S front panel to Raspberry Pi Pico

Build sheet. The reasoning behind every choice is in
[`01-hardware.md`](01-hardware.md); the MIDI protocol is in
[`03-midi.md`](03-midi.md).

Starting point: CDJ-100S disassembled, Mother Board removed, the front panel
`DISPLAY BOARD ASSY (DWG1503)` left in place and untouched.
Controller: Raspberry Pi Pico (RP2040), `PICO_BOARD=pico`.

## Four rules

1. **Feed CN601 pin 1 with 3.3 V, never 5 V.** It is labelled `V+5V` because
   the Mother Board fed it 5 V. Everything coming back out of the panel carries
   that voltage and the RP2040 is not 5 V tolerant. See `01-hardware.md` for the
   two less obvious reasons.
2. **Confirm which end of CN601 is pin 1** against service manual p.28-29
   before soldering. Getting it mirrored puts 3.3 V on `AC1`.
3. **One block at a time**: power, test; matrix, test; jog, test. Soldering
   everything before testing makes a fault impossible to localise.
4. **All three KD pins read 22 kΩ to ground**, so a multimeter cannot tell them
   apart. The only way to verify them is to press keys and read the firmware's
   output.

## Permanent modifications

Two, both reversible. No traces cut, no switches desoldered, nothing removed.

| # | Modification | Why | Reversible |
|---|---|---|---|
| 1 | 150 Ω in parallel with R611, R612, R613, R614 | LED brightness, 2.8 mA → ~8.5 mA | yes, unsolder them |
| 2 | D620's anode leg lifted off its pad | it is the one LED Pioneer wired permanently on | yes, solder it back |

For #1, stack an 0805 chip on top of the existing one. **In parallel with the
resistor, not the LED.**

## CN601 → Pico (16 wires)

`CN601` is a 36-pin FFC connector, Molex `52492-3620`, 1.0 mm pitch.

| CN601 | Signal | Pico GPIO | Phys. pin | Purpose |
|------:|--------|-----------|----------:|---------|
| 1 | V+5V | **3V3(OUT)** | 36 | panel supply, at 3.3 V |
| 2 | ADIN | GP26 / ADC0 | 31 | pitch fader wiper |
| 3 | CT | GP27 / ADC1 | 32 | fader centre tap (optional) |
| 4 | GNDD | GND | 38 | ground |
| 5 | JOG1 | GP10 | 14 | jog phase A |
| 6 | JOG2 | GP11 | 15 | jog phase B |
| 7 | KD2 | GP8 | 11 | matrix row 2 |
| 8 | KD1 | GP9 | 12 | matrix row 1 |
| 9 | KD0 | GP7 | 10 | matrix row 0 |
| 11 | S1 | GP2 | 4 | matrix column 1 |
| 12 | S2 | GP3 | 5 | matrix column 2 |
| 13 | S3 | GP4 | 6 | matrix column 3 |
| 14 | S4 | GP5 | 7 | matrix column 4 |
| 15 | S5 | GP6 | 9 | matrix column 5 |
| 33 | V-BV | GND | 8 | LED cathode return |
| 35 | GNDS | GND | 23 | ground (same node as pin 4, optional) |

Note KD1 and KD2: on the unit this was developed on they landed on GP9 and GP8,
crossed with respect to the obvious order. Which GPIO carries which row is
arbitrary as long as `kMatrixRowPins` in `board_config.h` agrees — wire a second
deck the same way and one binary serves both.

**Leave unconnected:** pins 10, 16-32, 34, 36. They only serve the FL tube.

## LEDs → Pico (5 wires, SIDE B)

| LED | Where to solder | Pico GPIO | Phys. pin |
|---|---|---|---|
| — | CN601 pin 33 (V-BV) to GND | GND | 8 |
| D614 `[PLAY]` | **anode pad**, beside switch S603, bottom right | GP16 | 21 |
| D616 `[CUE]` | **anode pad**, beside switch S606, right edge | GP17 | 22 |
| D618 `[DISC IND]` | **anode pad**, directly below jog encoder S614 | GP18 | 24 |
| D620 `[WINDOW]` | **lifted anode leg**, above and left of S614 | GP19 | 25 |

**Finding the anode:** measure resistance from each of the LED's two pads to
CN601 pin 33. The **cathode** reads the series resistor (470/470/360/330 Ω);
the **anode** reads open. For D620 the confirmation is that its anode reads 0 Ω
to the ground plane — that is exactly what has to be lifted.

Solder to the LED's own pad, not to the transistor: the anode pad *is* the
driver node (it is the Q601-Q603 emitter) and a through-hole pad is far easier
to hit than an SMD emitter.

No series resistor is needed on the GPIO — see `01-hardware.md`.

Do not bother matching LEDs to transistors. Which GPIO ends up on which LED is
decided by the `PIN_LED_*` defines, and the firmware's `l` self-test shows
immediately if two want swapping.

## Browse encoder (added part, not on CN601)

The CDJ-100S has none; the project adds one.

| Signal | Pico GPIO | Phys. pin |
|---|---|---|
| A | GP12 | 16 |
| B | GP13 | 17 |
| push (LOAD) | GP14 | 19 |
| common | GND | 18 |

## Full CN601 pinout

| Pin | Name | Pin | Name | Pin | Name |
|----:|------|----:|------|----:|------|
| 1 | V+5V | 13 | S3 | 25 | G8 |
| 2 | ADIN | 14 | S4 | 26 | G7 |
| 3 | CT | 15 | S5 | 27 | G6 |
| 4 | GNDD | 16 | S6 | 28 | G5 |
| 5 | JOG1 | 17 | S7 | 29 | G4 |
| 6 | JOG2 | 18 | S8 | 30 | G3 |
| 7 | KD2 | 19 | S9 | 31 | G2 |
| 8 | KD1 | 20 | S10 | 32 | G1 |
| 9 | KD0 | 21 | S11 | 33 | V-BV |
| 10 | S12 | 22 | G11 | 34 | AC2 |
| 11 | S1 | 23 | G10 | 35 | GNDS |
| 12 | S2 | 24 | G9 | 36 | AC1 |

## Bring-up

Test each block before soldering the next.

- [ ] **Power** (pins 1, 4, 35): 3.3 V between pin 1 and pin 4, nothing warm
- [ ] **Matrix** (pins 7-9, 11-15): all 13 buttons, no crosstalk
- [ ] **Jog** (pins 5-6): clockwise counts positive, count is monotonic
- [ ] **Pitch** (pins 2-3): smooth travel; note the min, max and detent values
- [ ] **LEDs** (pin 33 + 4 taps): `l` lights them in turn, `b` flashes
- [ ] **Browse encoder**: clockwise positive, push responds

### Matrix diagnostics

Press `r` on the serial console for the raw dump, one line per row:

```
RAW  KD0[.....] KD1[..X..] KD2[.....] | pitch=1802 ct=1795 | jog=214 ...
```

| Symptom | Almost certainly |
|---|---|
| a row permanently all `X` | that KD wire shorted to a column |
| a row permanently all `.` | that KD wire is off |
| wrong cell, **same row** | two S wires swapped |
| right cell, wrong row | two KD wires swapped |
| exactly one dead button | it is the only key on its column → that S wire |
| nothing at all | panel has no power |

`c` holds one column high permanently so you can measure the strobe on CN601
pins 11-15 with a multimeter: 3.3 V means the wire is good, 0 V means the joint
is not.

## Values measured on the reference unit

Re-measure these on yours; the firmware constants live in `board_config.h`.

| Quantity | Value |
|---|---|
| Pitch ADC, minimum | 43 |
| Pitch ADC, maximum | 4031 |
| Pitch ADC at the centre detent | **2019** |
| Pitch centre tap (CT) | 2040 |
| Jog, quadrature steps per revolution | ~96 (24 detents) |

The detent does **not** sit at the electrical centre of the track — 21 counts
apart on this unit. That is why the firmware maps the two halves of the travel
separately; a single linear map puts Mixxx's tempo at −0.09 % with the fader
detented.
