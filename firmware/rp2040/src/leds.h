#ifndef XDJ_LEDS_H
#define XDJ_LEDS_H

#include <stdbool.h>
#include <stdint.h>

// The four panel indicators. Only the first three have a driver transistor on
// the DISPLAY BOARD; D620 [WINDOW] has its anode wired straight to ground in
// the original, so it only works if that leg has been lifted and rewired.
// See the LED section of docs/rp2040-mod/01-hardware.md.
typedef enum {
    LED_PLAY = 0,    // D614, green
    LED_CUE,         // D616, yellow
    LED_DISC,        // D618, green  ("intern" in the upstream mapping)
    LED_WINDOW,      // D620, green  (needs the anode lifted, see above)
    LED_COUNT
} xdj_led_t;

void leds_init(void);

void led_set(xdj_led_t led, bool on);
bool led_get(xdj_led_t led);

// Blinks until the next led_set(). Used for the end-of-track warning the
// upstream firmware drives from Mixxx.
void led_blink(xdj_led_t led, uint32_t period_ms);

// One-shot flash: on now, off after ms. This is what a beat indicator wants —
// Mixxx sends a note per beat and the LED blips, rather than toggling at a
// duty cycle the firmware has to guess. Retriggering restarts the timer.
void led_pulse(xdj_led_t led, uint32_t ms);

// Advances the blink timers. Call about every millisecond.
void leds_task(void);

// Turns each LED on in turn, for wiring bring-up. Non-blocking: start it and
// keep calling leds_task(); it ends on its own.
void leds_start_self_test(void);

const char *led_name(xdj_led_t led);

#endif // XDJ_LEDS_H
