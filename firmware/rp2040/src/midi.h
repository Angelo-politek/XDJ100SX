#ifndef XDJ_MIDI_H
#define XDJ_MIDI_H

#include <stdbool.h>
#include <stdint.h>

#include "leds.h"
#include "matrix.h"

// MIDI protocol towards Mixxx. Note and CC numbers are identical to the
// original Teensy firmware's, so mixxx/MIDI/XDJ100SX.{js,midi.xml} works
// unchanged. Full table in docs/rp2040-mod/03-midi.md.

void midi_init(void);

// Drains incoming messages and applies them to the panel LEDs. Call often.
void midi_task(void);

// True once the host has bound the MIDI interface.
bool midi_ready(void);

// --- Deck to Mixxx --------------------------------------------------------

void midi_send_button(xdj_button_t button, bool pressed);
void midi_send_load_button(bool pressed);

// Take raw quadrature steps; the detent accounting happens inside, so these
// can be fed whatever encoder_take_delta() returns.
void midi_send_jog_steps(int32_t steps);
void midi_send_browse_steps(int32_t steps);

// 0..16383. Sends the MSB and LSB CCs, each only when it actually changes.
void midi_send_pitch(uint16_t value14);

// --- LED assignment -------------------------------------------------------
// Which indicator shows the beat and which shows end-of-track is a matter of
// taste and of what stays visible behind the new screen, so it is settable at
// runtime (the 'B' console command) rather than baked in.

void      midi_set_beat_led(xdj_led_t led);
xdj_led_t midi_beat_led(void);

#endif // XDJ_MIDI_H
