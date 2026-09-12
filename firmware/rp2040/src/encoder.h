#ifndef XDJ_ENCODER_H
#define XDJ_ENCODER_H

#include <stdbool.h>
#include <stdint.h>

// Interrupt driven quadrature decoding for the two encoders: the panel's jog
// wheel (S614, already RC filtered on the DISPLAY BOARD) and the browse
// encoder this project adds. Polling would be enough for the browse knob, but
// the jog wheel can be spun hard and a missed edge there is audible.
typedef enum {
    ENC_JOG = 0,
    ENC_BROWSE,
    ENC_COUNT
} xdj_encoder_t;

// Configures both encoder pin pairs and installs the shared GPIO callback.
void encoder_init(void);

// Absolute position since boot, in quadrature steps (4 per detent on a typical
// mechanical encoder). Signed, wraps like any int32.
int32_t encoder_position(xdj_encoder_t enc);

// Steps accumulated since the previous call to this function, then clears the
// accumulator. This is what the MIDI layer will turn into a relative CC.
int32_t encoder_take_delta(xdj_encoder_t enc);

// Debounced state of the browse encoder's push switch (active low on the pin,
// reported true while held). Call browse_switch_update() about every ms.
void browse_switch_update(void);
bool browse_switch_pressed(void);

// True exactly once per press, for edge driven callers.
bool browse_switch_take_press(void);

#endif // XDJ_ENCODER_H
