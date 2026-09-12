#ifndef XDJ_BOARD_CONFIG_H
#define XDJ_BOARD_CONFIG_H

// Pin map for the Raspberry Pi Pico <-> CDJ-100S DISPLAY BOARD ASSY (DWG1503).
// Full wiring table and the reasoning behind it:
//   docs/rp2040-mod/02-wiring.md
//   docs/rp2040-mod/01-hardware.md
//
// The panel is fed 3.3 V on CN601 pin 1 (labelled V+5V on the schematic).
// Everything coming back out of it is then 3.3 V safe.

// --- Key matrix -----------------------------------------------------------
// Columns are the FL segment lines S1..S5 (CN601 pins 11..15), driven high one
// at a time. Rows are KD0..KD2 (CN601 pins 9/8/7), each with a 22k pulldown on
// the panel, so a pressed key reads HIGH.

#define MATRIX_COLS 5
#define MATRIX_ROWS 3

// Index 0..4 -> S1..S5 -> CN601 pins 11,12,13,14,15
static const uint8_t kMatrixColPins[MATRIX_COLS] = { 2, 3, 4, 5, 6 };
// Index 0..2 -> KD0, KD1, KD2.
// Mind the GPIO order. On this unit CN601 pin 8 (KD1) landed on GP9 and pin 7
// (KD2) on GP8: the two wires are crossed with respect to the obvious order.
// Which GPIO carries which row is arbitrary as long as this array says so, so
// the firmware follows the harness. If the harness is ever rebuilt with pin 8
// on GP8 and pin 7 on GP9, change this back to { 7, 8, 9 }.
static const uint8_t kMatrixRowPins[MATRIX_ROWS] = { 7, 9, 8 };

// Settling time after raising a column, before sampling the rows. The rows see
// a ~22k pulldown plus wiring capacitance; 20 us is already generous.
#define MATRIX_SETTLE_US 20

// A key must read the same for this many consecutive scans to change state.
// The scan runs every 1 ms, so this is the debounce time in milliseconds.
// The upstream Teensy build used 50 ms, which is sluggish for CUE drumming.
#define MATRIX_DEBOUNCE_SCANS 4

// --- Jog wheel encoder (S614, on the panel) -------------------------------
// CN601 pins 5 and 6. Already pulled up to V+5V and RC filtered on the panel,
// so no internal pull-up is needed (enabling one is harmless).
#define PIN_JOG_A 10  // CN601 pin 5, JOG1
#define PIN_JOG_B 11  // CN601 pin 6, JOG2

// --- Browse encoder (added by this project, not on the CDJ-100S) ----------
#define PIN_BROWSE_A  12
#define PIN_BROWSE_B  13
#define PIN_BROWSE_SW 14  // push to load, active low, internal pull-up

// --- Pitch fader (VR601, 10k linear, via R622/C602) -----------------------
#define PIN_PITCH_ADIN 26  // CN601 pin 2 -> ADC0
#define PIN_PITCH_CT   27  // CN601 pin 3 -> ADC1 (centre tap, optional wire)
#define ADC_CH_PITCH_ADIN 0
#define ADC_CH_PITCH_CT   1

// Fader travel, measured on the reference deck (see docs/rp2040-mod/02-wiring.md).
// The panel runs on 3.3 V and the fader spans almost the whole ADC range:
// D613 on the schematic is a clamp, not in series with the wiper.
#define PITCH_ADC_MIN 43
#define PITCH_ADC_MAX 4037

// ADC value with the fader parked in its centre detent, measured on deck 1.
// It is NOT the midpoint of the travel: the detent is mechanical and the
// track's electrical centre (read on the centre tap, CN601 pin 3) sits at 2040,
// 21 counts away. Mapping the two halves of the travel separately is what makes
// the detent land on exactly 0 % in Mixxx instead of -0.09 %.
#define PITCH_ADC_CENTER 2019

// Counts either side of the detent that report exactly 0 %. Small enough to be
// imperceptible, big enough to absorb ADC noise and the detent's own play.
#define PITCH_CENTER_DEADZONE 4

// The mechanical end stops do not land on the same count twice — a few tens of
// counts of spread between runs is normal. Treat anything inside these margins
// as a hard end, otherwise the fader never reliably reaches 0 % and 100 %.
#define PITCH_END_DEADZONE 25

// --- MIDI ------------------------------------------------------------------
// Quadrature steps per MIDI message. A mechanical encoder produces four steps
// per detent, so 4 means one message per click, matching the original Teensy
// firmware. Lowering the jog value to 1 quadruples its resolution but diverges
// from the upstream mapping's feel.
// Hysteresis on the pitch value actually sent, in 14-bit units. One ADC count
// is about 4 units, and the ADC dithers by a count even when the fader is
// still: without this the deck streams a CC on every update forever. 12 units
// is under 0.1 % of fader travel, far below what the fader resolves.
#define PITCH_MIDI_HYSTERESIS 12

#define JOG_STEPS_PER_MIDI_TICK    4
#define BROWSE_STEPS_PER_MIDI_TICK 4

// Which indicator shows the beat and which the end of track. Changeable at
// runtime with the 'B' console command; these are just the defaults, matching
// the upstream firmware's pin order.
#define LED_FOR_BEAT          LED_WINDOW
#define LED_FOR_END_OF_TRACK  LED_DISC

// --- LEDs ------------------------------------------------------------------
// Not reachable through CN601: these are tapped on the LED anode pads, and
// D620 needs its anode leg lifted. See the LED section of docs/rp2040-mod/01-hardware.md.
#define PIN_LED_PLAY   16
#define PIN_LED_CUE    17
#define PIN_LED_DISC   18
#define PIN_LED_WINDOW 19

#endif // XDJ_BOARD_CONFIG_H
