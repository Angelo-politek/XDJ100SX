#ifndef XDJ_PITCH_H
#define XDJ_PITCH_H

#include <stdbool.h>
#include <stdint.h>

// Pitch fader (VR601, 10k linear) read on ADC0 through the panel's own
// R622/C602 filter, plus the centre tap on ADC1.
//
// The usable span is not 0..4095: the top of the track sits one diode drop
// (D613) below the 3.3 V rail, so expect roughly 0..3500 counts. Rather than
// hard-coding a guess, this module reports the extremes it has actually seen,
// which is what the bring-up session is for.

void pitch_init(void);

// Takes an oversampled reading. Call at a few hundred Hz; more often just
// burns cycles, the RC on the panel is slower than that anyway.
void pitch_update(void);

// Latest filtered value, 0..4095.
uint16_t pitch_raw(void);

// Calibrated position, 0..16383, ready to be split into a 14-bit MIDI CC pair.
// Uses PITCH_ADC_MIN/MAX from board_config.h and clamps the end deadzones, so
// both ends of the fader travel reach exactly 0 and 16383 every time.
uint16_t pitch_value14(void);

// Latest centre tap reading, 0..4095. Only meaningful if CN601 pin 3 is wired.
uint16_t pitch_centre_tap_raw(void);

// Extremes seen since boot, for calibration during bring-up.
uint16_t pitch_seen_min(void);
uint16_t pitch_seen_max(void);

// There is deliberately no "has it changed" flag here. Whether a value is worth
// acting on depends on what the caller last acted on, so each consumer (the
// serial log, the MIDI sender) keeps its own last-sent value and compares
// against pitch_value14(). A flag owned by this module gets consumed by
// whoever asks first, even when that caller then decides not to send — and the
// fader's final resting value is exactly the one that gets swallowed.

#endif // XDJ_PITCH_H
