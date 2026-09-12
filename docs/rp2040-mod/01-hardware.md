# CDJ-100S front panel — how it actually works

Everything here is read off the Pioneer service manual `RRV2027`
([`datasheets/pioneer_cdj-100s-sm-rrv2027.pdf`](../../datasheets/pioneer_cdj-100s-sm-rrv2027.pdf))
and then confirmed on a real unit. It is worth reading even if you are building
the Teensy version: it documents what the front panel does on its own.

| Page | Content |
|---|---|
| 22 | Display Board schematic, left half — CN601 pinout, LED drivers, jog encoder |
| 23 | Display Board schematic, right half — key matrix, pitch slider, LEDs |
| 28-29 | PCB component placement, both sides |
| 32 | Display Board parts list |
| 40 | CPU pin functions (KD0-KD2 = "Key-scan data input") |

## The short version

The front panel (`DISPLAY BOARD ASSY`, `DWG1503`) is self-contained: 13
buttons, the jog encoder, the pitch fader, four LEDs, the FL tube and its
74HC175 LED latch. It talks to the Mother Board through **one** connector,
`CN601`, a 36-pin FFC.

The original CPU did not read one button per pin — it **scanned a matrix**.
With the Mother Board removed those lines are free, so an RP2040 can take the
CPU's place and drive the matrix exactly as it did. No traces cut, no switches
desoldered.

## The key matrix

13 buttons on **5 columns × 3 rows**.

- **Columns** are the FL segment lines `S1`-`S5`, each entering the matrix
  through its own isolation diode (1SS355). The strobe is **active high**.
- **Rows** are `KD0`-`KD2`, each with a 22 kΩ pulldown to GNDS
  (R616/R617/R618).

|  | S1 | S2 | S3 | S4 | S5 |
|---|---|---|---|---|---|
| **KD0** | HOLD `S615` | TIME/AUTO CUE `S613` | EJECT `S611` | MASTER TEMPO `S608` | — |
| **KD1** | TRACK ◀◀ `S601` | TRACK ▶▶ `S604` | JET `S607` | ZIP `S609` | WAH `S610` |
| **KD2** | PLAY/PAUSE `S603` | CUE `S606` | SEARCH ◀◀ `S602` | SEARCH ▶▶ `S605` | — |

Two consequences for firmware:

- The diodes point *into* the matrix, so pulling a column low simply reverse
  biases its diode and the column floats. The scan needs no special handling.
- The switches have **no** per-key diode, so rollover is limited to two keys.
  Three keys forming an "L" fabricate a fourth. This firmware detects that and
  freezes the affected columns rather than reporting a key nobody pressed.

## Run the panel at 3.3 V, not 5 V

CN601 pin 1 is labelled `V+5V` because the Mother Board fed it 5 V. Feed it
**3.3 V**. This is not generic caution — it holds three things together:

1. **Inputs.** `KD0`-`KD2`, `JOG1`/`JOG2` and `ADIN` come back at whatever
   voltage the panel runs on, and the RP2040 is not 5 V tolerant.
2. **Logic threshold.** A strobe reaches the row through one diode drop, so a
   pressed key puts about 2.8 V on KDx. The RP2040's V_IH at 3.3 V is 2.31 V —
   comfortable.
3. **The LED drivers**, below: it is the reason the panel's transistors cannot
   fight the GPIOs.

Panel draw is a couple of milliamps plus LED current; the Pico's own 3V3
regulator handles it.

## Jog and pitch

**Jog encoder** `S614` (`DSX1051`), mechanical quadrature. Common to GNDS,
phases already pulled up by R626/R627 (10 kΩ) and RC filtered by R624/R625
(10 kΩ) with C614/C615 (10 nF) before reaching CN601 pins 5/6. Idle high.
Measured: **~96 quadrature steps per revolution** (24 detents).

**Pitch fader** `VR601` (`DCV1009`), 10 kΩ linear with a centre tap. The wiper
is filtered by R622 (10 kΩ) + C602 (10 nF) onto `ADIN`; the centre tap comes
out on `CT`. At 3.3 V it covers nearly the whole ADC range — measured 43…4031.
D613 on the schematic is a clamp, **not** in series with the wiper.

**The detent is not the electrical centre.** On the unit tested, the wiper at
the centre detent reads 2019 while the centre tap reads 2040. Twenty-one counts
of manufacturing offset — enough to put Mixxx's tempo at −0.09 % instead of
0.0 % if you map the travel with a single straight line. Map the two halves
separately, hinged on the measured detent value.

## The LEDs

Four LEDs, **only three of them driven**:

| LED | Function | Series R | Driven by |
|---|---|---|---|
| D614 | `[PLAY]`, green | R611 470 Ω | Q601 |
| D616 | `[CUE]`, yellow | R612 470 Ω | Q602 |
| D618 | `[DISC IND]`, green | R613 360 Ω | Q603 |
| D620 | `[WINDOW]`, green | R614 330 Ω | **nothing — anode wired to GNDS** |

`IC602` (HD74HC175FP) is a quad flip-flop but only three channels are used:
each Q output drives a PDTA124EK digital PNP (Q608-Q610), which drives a
2SC2412K NPN. D620 was never meant to be controlled — Pioneer wired it
permanently on as the display-window illumination.

### Why pin 33 alone is not enough

Each NPN has its **collector on GNDS** and its **emitter on the LED anode**:

```
                     +-- Q601 collector -- GNDS (0 V)
   base <--R601 1k-- |
                     +-- Q601 emitter --> LED anode -->|-- R611 470R -- V-BV
                                                                         ^
                                              about -8 V, from the Mother
                                              Board we just removed
```

So the transistor switches the anode down to **0 V** and the LED current
returns to a **negative** rail. D620 proves the rail is negative: its anode
sits on GNDS and it is lit in normal operation, which is only possible if the
cathode side is below zero.

With the Mother Board gone, tying pin 33 to 0 V puts 0 V on both ends of every
LED. The anode nodes are not on CN601, so **tap wires on the board are
unavoidable** — see [`02-wiring.md`](02-wiring.md).

### Why the GPIO always wins

`IC602` powers up with undefined outputs, so a transistor may be holding an
anode at 0 V. It does not matter: the panel is on the same 3.3 V rail as the
RP2040 and the NPN's base is fed from that rail through 1 kΩ, so it can never
be pulled above our own logic high.

- GPIO at 3.3 V: the base-collector junction clamps the base near 0.7 V, so
  V_BE ≈ −2.6 V. The transistor is off and no emitter current flows. About
  2.6 mA leaks from the rail through the PNP and R601 to ground — wasteful,
  harmless.
- GPIO at 0 V: the base sits at 0.7 V and the GPIO sinks about 2.6 mA of base
  current. Also harmless.

At 5 V the reverse V_BE would be about 4.3 V, right at the ~5 V V_EBO limit of
a small-signal NPN. One more reason for 3.3 V.

### Brightness

With 3.3 V driving a ~2.1 V forward drop there is barely 1.2 V across the
series resistor: about **2.8 mA**, and these are 1998 GaP parts. Visible but
dim. Fix it with a second resistor **in parallel** with each original — nothing
to desolder, fully reversible. 150 Ω brings 470 Ω down to 114 Ω, about 8.5 mA,
inside the RP2040's 12 mA per-pin budget.

## Parts referenced

| Ref | What | Value / code |
|---|---|---|
| CN601 | FFC connector, 36 pins | Molex 52492-3620, 1.0 mm pitch |
| — | original FFC cable | DDD1131, 36P/60V |
| VR601 | pitch fader | 10 kΩ linear (B), DCV1009 |
| S614 | jog encoder | DSX1051 |
| V601 | FL tube | DEL1031 |
| IC602 | LED latch (unused here) | HD74HC175FP |
| Q601-Q603 | LED drivers | 2SC2412K |
| Q608-Q610 | digital PNPs | PDTA124EK |
| R611/R612/R613/R614 | LED series | 470 / 470 / 360 / 330 Ω |
| R616-R618 | KD0-KD2 pulldowns | 22 kΩ |
| R622 | ADIN series | 10 kΩ |
| R624-R627 | jog filter and pull-ups | 10 kΩ |
| D625/D606/D608/D607/D691 | S1-S5 column isolation | 1SS355 |

⚠️ The schematic on p.23 labels the LED drivers **Q501-Q503**, while the note on
the same page, the parts list and the PCB silkscreen all say **Q601-Q603**. The
schematic designators are a typo.
