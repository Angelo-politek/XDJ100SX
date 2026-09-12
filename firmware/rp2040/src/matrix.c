#include "matrix.h"

#include "pico/stdlib.h"
#include "hardware/gpio.h"

#include "board_config.h"

// Physical position of each button in the scanned matrix. Rows are KD0..KD2,
// columns are the segment strobes S1..S5. Two positions are unused (the
// CDJ-100S only has 13 of the 15 possible keys).
#define NO_KEY ((int8_t)-1)

static const int8_t kLayout[MATRIX_ROWS][MATRIX_COLS] = {
    /* KD0 */ { BTN_HOLD,       BTN_TIME,       BTN_EJECT,       BTN_MASTER_TEMPO, NO_KEY   },
    /* KD1 */ { BTN_TRACK_PREV, BTN_TRACK_NEXT, BTN_JET,         BTN_ZIP,          BTN_WAH  },
    /* KD2 */ { BTN_PLAY,       BTN_CUE,        BTN_SEARCH_BACK, BTN_SEARCH_FWD,   NO_KEY   },
};

static const char *const kNames[BTN_COUNT] = {
    "HOLD", "TIME", "EJECT", "MASTER_TEMPO",
    "TRACK_PREV", "TRACK_NEXT", "JET", "ZIP", "WAH",
    "PLAY", "CUE", "SEARCH_BACK", "SEARCH_FWD",
};

static bool    s_state[BTN_COUNT];    // debounced
static uint8_t s_counter[BTN_COUNT];  // consecutive scans disagreeing with s_state
static uint16_t s_raw_bitmap;
static bool    s_ghost;
static int     s_force_col = -1;  // -1 = normal scanning

// Small ring buffer of edges, drained by the caller.
#define EVENT_QUEUE_LEN 32
static struct {
    uint8_t button;
    bool    pressed;
} s_events[EVENT_QUEUE_LEN];
static uint8_t s_ev_head, s_ev_tail;

static void push_event(uint8_t button, bool pressed) {
    uint8_t next = (uint8_t)((s_ev_head + 1) % EVENT_QUEUE_LEN);
    if (next == s_ev_tail) {
        return;  // full: drop the oldest-to-be rather than corrupt the queue
    }
    s_events[s_ev_head].button = button;
    s_events[s_ev_head].pressed = pressed;
    s_ev_head = next;
}

static uint8_t popcount5(uint8_t v) {
    uint8_t n = 0;
    while (v) {
        n = (uint8_t)(n + (v & 1u));
        v = (uint8_t)(v >> 1);
    }
    return n;
}

void matrix_init(void) {
    for (int c = 0; c < MATRIX_COLS; c++) {
        gpio_init(kMatrixColPins[c]);
        gpio_set_dir(kMatrixColPins[c], GPIO_OUT);
        gpio_put(kMatrixColPins[c], 0);
    }
    for (int r = 0; r < MATRIX_ROWS; r++) {
        gpio_init(kMatrixRowPins[r]);
        gpio_set_dir(kMatrixRowPins[r], GPIO_IN);
        // The panel already pulls each row down with 22k (R616/R617/R618). The
        // internal pulldown (~60k) pulls the same way, so it changes nothing
        // once wired — a strobe still reaches ~2.8 V through the parallel
        // combination — but it keeps the rows from floating and inventing key
        // presses while the panel is not connected yet.
        gpio_pull_down(kMatrixRowPins[r]);
    }

    for (int b = 0; b < BTN_COUNT; b++) {
        s_state[b] = false;
        s_counter[b] = 0;
    }
    s_raw_bitmap = 0;
    s_ghost = false;
    s_force_col = -1;
    s_ev_head = s_ev_tail = 0;
}

void matrix_force_column(int col) {
    s_force_col = (col >= 0 && col < MATRIX_COLS) ? col : -1;
    for (int c = 0; c < MATRIX_COLS; c++) {
        gpio_put(kMatrixColPins[c], s_force_col == c);
    }
}

int matrix_forced_column(void) {
    return s_force_col;
}

void matrix_scan(void) {
    uint8_t row_mask[MATRIX_ROWS] = { 0 };  // one bit per column, currently closed

    if (s_force_col >= 0) {
        // Diagnostic mode: the column is already held high by
        // matrix_force_column(), so only sample the rows. No debounce, no
        // events — this exists to be probed with a multimeter, not played.
        s_ghost = false;
        s_raw_bitmap = 0;
        for (int r = 0; r < MATRIX_ROWS; r++) {
            if (gpio_get(kMatrixRowPins[r])) {
                s_raw_bitmap = (uint16_t)(s_raw_bitmap |
                                          (1u << (r * MATRIX_COLS + s_force_col)));
            }
        }
        return;
    }

    for (int c = 0; c < MATRIX_COLS; c++) {
        gpio_put(kMatrixColPins[c], 1);
        busy_wait_us_32(MATRIX_SETTLE_US);

        for (int r = 0; r < MATRIX_ROWS; r++) {
            if (gpio_get(kMatrixRowPins[r])) {
                row_mask[r] = (uint8_t)(row_mask[r] | (1u << c));
            }
        }

        gpio_put(kMatrixColPins[c], 0);
    }

    // Publish the raw picture before any filtering, so the bring-up dump shows
    // exactly what the hardware returned.
    uint16_t bitmap = 0;
    for (int r = 0; r < MATRIX_ROWS; r++) {
        for (int c = 0; c < MATRIX_COLS; c++) {
            if (row_mask[r] & (1u << c)) {
                bitmap = (uint16_t)(bitmap | (1u << (r * MATRIX_COLS + c)));
            }
        }
    }
    s_raw_bitmap = bitmap;

    // The switches have no per-key diodes, so three keys sharing two columns
    // across two rows fabricate a fourth. Those columns cannot be trusted this
    // scan; freeze them instead of reporting a key nobody pressed.
    uint8_t ambiguous[MATRIX_ROWS] = { 0 };
    s_ghost = false;
    for (int a = 0; a < MATRIX_ROWS; a++) {
        for (int b = a + 1; b < MATRIX_ROWS; b++) {
            uint8_t common = (uint8_t)(row_mask[a] & row_mask[b]);
            if (popcount5(common) >= 2) {
                ambiguous[a] = (uint8_t)(ambiguous[a] | common);
                ambiguous[b] = (uint8_t)(ambiguous[b] | common);
                s_ghost = true;
            }
        }
    }

    for (int r = 0; r < MATRIX_ROWS; r++) {
        for (int c = 0; c < MATRIX_COLS; c++) {
            int8_t button = kLayout[r][c];
            if (button == NO_KEY || (ambiguous[r] & (1u << c))) {
                continue;
            }

            bool sample = (row_mask[r] & (1u << c)) != 0;
            if (sample == s_state[button]) {
                s_counter[button] = 0;
                continue;
            }

            if (++s_counter[button] >= MATRIX_DEBOUNCE_SCANS) {
                s_counter[button] = 0;
                s_state[button] = sample;
                push_event((uint8_t)button, sample);
            }
        }
    }
}

bool matrix_is_pressed(xdj_button_t button) {
    return (button < BTN_COUNT) ? s_state[button] : false;
}

bool matrix_pop_event(xdj_button_t *button, bool *pressed) {
    if (s_ev_tail == s_ev_head) {
        return false;
    }
    *button = (xdj_button_t)s_events[s_ev_tail].button;
    *pressed = s_events[s_ev_tail].pressed;
    s_ev_tail = (uint8_t)((s_ev_tail + 1) % EVENT_QUEUE_LEN);
    return true;
}

uint16_t matrix_raw_bitmap(void) {
    return s_raw_bitmap;
}

bool matrix_ghost_detected(void) {
    return s_ghost;
}

const char *matrix_button_name(xdj_button_t button) {
    return (button < BTN_COUNT) ? kNames[button] : "?";
}
