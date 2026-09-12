#include "leds.h"

#include "pico/stdlib.h"
#include "hardware/gpio.h"

#include "board_config.h"

// Each GPIO drives an LED anode node directly. The panel runs on the same
// 3.3 V rail as the RP2040, which is what makes this safe: the on-board driver
// transistor's base can never be pulled above our own logic high, so it cannot
// fight the GPIO. Full reasoning in docs/rp2040-mod/01-hardware.md.

#define SELF_TEST_STEP_MS 300

static const uint8_t kLedPins[LED_COUNT] = {
    PIN_LED_PLAY,
    PIN_LED_CUE,
    PIN_LED_DISC,
    PIN_LED_WINDOW,
};

static const char *const kLedNames[LED_COUNT] = {
    "PLAY", "CUE", "DISC", "WINDOW",
};

typedef struct {
    bool     on;
    uint32_t blink_period_ms;    // 0 = steady
    uint32_t blink_timer_ms;
    uint32_t pulse_remaining_ms; // 0 = not pulsing
} led_state_t;

static led_state_t s_led[LED_COUNT];
static int         s_self_test_step = -1;  // -1 = idle
static uint32_t    s_self_test_timer;

static void drive(xdj_led_t led, bool on) {
    gpio_put(kLedPins[led], on);
}

void leds_init(void) {
    for (int i = 0; i < LED_COUNT; i++) {
        gpio_init(kLedPins[i]);
        gpio_set_dir(kLedPins[i], GPIO_OUT);
        // The panel's LEDs sit on a 3.3 V rail with barely a volt of headroom
        // above their forward drop, so every millivolt of output droop costs
        // real brightness. The default 4 mA drive sags well before the series
        // resistor is the limit; 12 mA keeps the pin stiff.
        gpio_set_drive_strength(kLedPins[i], GPIO_DRIVE_STRENGTH_12MA);
        gpio_put(kLedPins[i], 0);
        s_led[i].on = false;
        s_led[i].blink_period_ms = 0;
        s_led[i].blink_timer_ms = 0;
        s_led[i].pulse_remaining_ms = 0;
    }
    s_self_test_step = -1;
}

void led_set(xdj_led_t led, bool on) {
    if (led >= LED_COUNT) {
        return;
    }
    s_led[led].blink_period_ms = 0;
    s_led[led].pulse_remaining_ms = 0;
    s_led[led].on = on;
    drive(led, on);
}

void led_pulse(xdj_led_t led, uint32_t ms) {
    if (led >= LED_COUNT || ms == 0) {
        return;
    }
    s_led[led].blink_period_ms = 0;
    s_led[led].pulse_remaining_ms = ms;
    s_led[led].on = true;
    drive(led, true);
}

bool led_get(xdj_led_t led) {
    return (led < LED_COUNT) ? s_led[led].on : false;
}

void led_blink(xdj_led_t led, uint32_t period_ms) {
    if (led >= LED_COUNT || period_ms == 0) {
        return;
    }
    s_led[led].blink_period_ms = period_ms;
    s_led[led].blink_timer_ms = 0;
}

void leds_start_self_test(void) {
    for (int i = 0; i < LED_COUNT; i++) {
        led_set((xdj_led_t)i, false);
    }
    s_self_test_step = 0;
    s_self_test_timer = 0;
}

void leds_task(void) {
    for (int i = 0; i < LED_COUNT; i++) {
        if (s_led[i].pulse_remaining_ms) {
            if (--s_led[i].pulse_remaining_ms == 0) {
                s_led[i].on = false;
                drive((xdj_led_t)i, false);
            }
            continue;
        }
        if (s_led[i].blink_period_ms == 0) {
            continue;
        }
        if (++s_led[i].blink_timer_ms >= s_led[i].blink_period_ms / 2) {
            s_led[i].blink_timer_ms = 0;
            s_led[i].on = !s_led[i].on;
            drive((xdj_led_t)i, s_led[i].on);
        }
    }

    if (s_self_test_step < 0) {
        return;
    }
    if (++s_self_test_timer < SELF_TEST_STEP_MS) {
        return;
    }
    s_self_test_timer = 0;
    if (s_self_test_step > 0) {
        led_set((xdj_led_t)(s_self_test_step - 1), false);
    }
    if (s_self_test_step >= LED_COUNT) {
        s_self_test_step = -1;  // done
        return;
    }
    led_set((xdj_led_t)s_self_test_step, true);
    s_self_test_step++;
}

const char *led_name(xdj_led_t led) {
    return (led < LED_COUNT) ? kLedNames[led] : "?";
}
