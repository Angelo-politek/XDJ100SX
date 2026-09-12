#include "encoder.h"

#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/sync.h"

#include "board_config.h"

// Classic 4x quadrature transition table, indexed by (previous << 2) | current
// where each state is (A << 1) | B. Invalid transitions (both phases changed,
// i.e. a missed edge) yield 0 rather than guessing a direction.
static const int8_t kQuadTable[16] = {
     0, -1,  1,  0,
     1,  0,  0, -1,
    -1,  0,  0,  1,
     0,  1, -1,  0,
};

typedef struct {
    uint8_t pin_a;
    uint8_t pin_b;
    uint8_t last_state;
    volatile int32_t position;
    volatile int32_t delta;
} encoder_t;

static encoder_t s_enc[ENC_COUNT] = {
    [ENC_JOG]    = { .pin_a = PIN_JOG_A,    .pin_b = PIN_JOG_B    },
    [ENC_BROWSE] = { .pin_a = PIN_BROWSE_A, .pin_b = PIN_BROWSE_B },
};

static bool    s_sw_state;
static uint8_t s_sw_counter;
static volatile bool s_sw_press_pending;

static uint8_t read_state(const encoder_t *e) {
    return (uint8_t)((gpio_get(e->pin_a) ? 2u : 0u) | (gpio_get(e->pin_b) ? 1u : 0u));
}

static void update(encoder_t *e) {
    uint8_t now = read_state(e);
    int8_t step = kQuadTable[(e->last_state << 2) | now];
    e->last_state = now;
    if (step) {
        e->position += step;
        e->delta += step;
    }
}

// One callback serves every encoder pin; work out which encoder moved from the
// pin number rather than keeping four separate handlers.
static void gpio_callback(uint gpio, uint32_t events) {
    (void)events;
    for (int i = 0; i < ENC_COUNT; i++) {
        if (gpio == s_enc[i].pin_a || gpio == s_enc[i].pin_b) {
            update(&s_enc[i]);
            return;
        }
    }
}

void encoder_init(void) {
    for (int i = 0; i < ENC_COUNT; i++) {
        gpio_init(s_enc[i].pin_a);
        gpio_init(s_enc[i].pin_b);
        gpio_set_dir(s_enc[i].pin_a, GPIO_IN);
        gpio_set_dir(s_enc[i].pin_b, GPIO_IN);
        // The jog wheel already has 10k pull-ups on the panel; the added browse
        // encoder has none. Enabling the internal pull-up on both is correct
        // either way (it just parallels the panel's resistors).
        gpio_pull_up(s_enc[i].pin_a);
        gpio_pull_up(s_enc[i].pin_b);
        s_enc[i].position = 0;
        s_enc[i].delta = 0;
    }

    gpio_init(PIN_BROWSE_SW);
    gpio_set_dir(PIN_BROWSE_SW, GPIO_IN);
    gpio_pull_up(PIN_BROWSE_SW);
    s_sw_state = false;
    s_sw_counter = 0;
    s_sw_press_pending = false;

    // Settle the pins, then latch the starting state so the first edge does not
    // produce a bogus step.
    sleep_ms(2);
    for (int i = 0; i < ENC_COUNT; i++) {
        s_enc[i].last_state = read_state(&s_enc[i]);
    }

    gpio_set_irq_enabled_with_callback(s_enc[0].pin_a,
                                       GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL,
                                       true, &gpio_callback);
    gpio_set_irq_enabled(s_enc[0].pin_b, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true);
    for (int i = 1; i < ENC_COUNT; i++) {
        gpio_set_irq_enabled(s_enc[i].pin_a, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true);
        gpio_set_irq_enabled(s_enc[i].pin_b, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true);
    }
}

int32_t encoder_position(xdj_encoder_t enc) {
    return (enc < ENC_COUNT) ? s_enc[enc].position : 0;
}

int32_t encoder_take_delta(xdj_encoder_t enc) {
    if (enc >= ENC_COUNT) {
        return 0;
    }
    // The ISR writes delta, so read-and-clear has to be atomic against it.
    uint32_t save = save_and_disable_interrupts();
    int32_t d = s_enc[enc].delta;
    s_enc[enc].delta = 0;
    restore_interrupts(save);
    return d;
}

void browse_switch_update(void) {
    bool sample = !gpio_get(PIN_BROWSE_SW);  // active low
    if (sample == s_sw_state) {
        s_sw_counter = 0;
        return;
    }
    if (++s_sw_counter >= MATRIX_DEBOUNCE_SCANS) {
        s_sw_counter = 0;
        s_sw_state = sample;
        if (sample) {
            s_sw_press_pending = true;
        }
    }
}

bool browse_switch_pressed(void) {
    return s_sw_state;
}

bool browse_switch_take_press(void) {
    if (!s_sw_press_pending) {
        return false;
    }
    s_sw_press_pending = false;
    return true;
}
