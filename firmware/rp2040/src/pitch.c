#include "pitch.h"

#include "pico/stdlib.h"
#include "hardware/adc.h"

#include "board_config.h"

// Averaging 16 samples buys two extra bits of stability for free; the RP2040
// ADC is noisy enough that a single reading jitters by several LSB.
#define PITCH_OVERSAMPLE 16

// Adaptive smoothing, the same idea as the ResponsiveAnalogRead library the
// original Teensy firmware used. A carbon wiper chatters, and at 14-bit MIDI
// resolution one ADC count is four MIDI steps, so raw values would flood Mixxx
// with jitter. Small changes are filtered hard (the fader is standing still,
// only noise is moving); a change bigger than the snap threshold is real
// movement and is followed immediately.
#define PITCH_SNAP_DELTA 64  // counts; above this, follow the input directly
#define PITCH_EMA_SHIFT  3   // otherwise move an eighth of the way per update

// The filter accumulator carries four fractional bits. Without them a step of
// (delta >> 3) rounds to zero for any delta below 8 and the filter stalls short
// of its target — and it stalls asymmetrically, because an arithmetic shift
// rounds negative values away from zero but positive ones toward it.
#define PITCH_FRAC_BITS 4

// The centre tap is a slow, rarely used signal. Reading it on every update
// doubles the ADC work and gives charge from the other input more chances to
// contaminate the pitch reading, so sample it once in a while instead.
#define PITCH_CT_DIVIDER 10

static int32_t  s_accum;       // s_value << PITCH_FRAC_BITS
static uint16_t s_value;
static uint16_t s_centre_tap;
static uint8_t  s_ct_counter;
static uint16_t s_min = 0xFFFF;
static uint16_t s_max;

static uint16_t read_channel(uint channel) {
    adc_select_input(channel);
    // Throw the first conversion away: right after a channel switch the sample
    // capacitor still carries charge from the previous input, and with the
    // panel's 10 k series resistor it does not settle within one conversion.
    (void)adc_read();

    uint32_t sum = 0;
    for (int i = 0; i < PITCH_OVERSAMPLE; i++) {
        sum += adc_read();
    }
    return (uint16_t)(sum / PITCH_OVERSAMPLE);
}

void pitch_init(void) {
    adc_init();
    adc_gpio_init(PIN_PITCH_ADIN);
    adc_gpio_init(PIN_PITCH_CT);

    s_value = read_channel(ADC_CH_PITCH_ADIN);
    s_accum = (int32_t)s_value << PITCH_FRAC_BITS;
    s_centre_tap = read_channel(ADC_CH_PITCH_CT);
    s_ct_counter = 0;
    s_min = s_value;
    s_max = s_value;
}

void pitch_update(void) {
    uint16_t sample = read_channel(ADC_CH_PITCH_ADIN);

    if (++s_ct_counter >= PITCH_CT_DIVIDER) {
        s_ct_counter = 0;
        s_centre_tap = read_channel(ADC_CH_PITCH_CT);
    }

    int delta = (int)sample - (int)s_value;
    int magnitude = (delta < 0) ? -delta : delta;
    int32_t target = (int32_t)sample << PITCH_FRAC_BITS;

    if (magnitude >= PITCH_SNAP_DELTA) {
        s_accum = target;                                  // real movement
    } else {
        s_accum += (target - s_accum) >> PITCH_EMA_SHIFT;  // noise
    }
    s_value = (uint16_t)(s_accum >> PITCH_FRAC_BITS);

    if (s_value < s_min) {
        s_min = s_value;
    }
    if (s_value > s_max) {
        s_max = s_value;
    }
}

uint16_t pitch_value14(void) {
    const int lo = PITCH_ADC_MIN + PITCH_END_DEADZONE;
    const int hi = PITCH_ADC_MAX - PITCH_END_DEADZONE;
    const int centre = PITCH_ADC_CENTER;
    const int mid14 = 8192;
    int v = (int)s_value;

    if (v <= lo) {
        return 0;
    }
    if (v >= hi) {
        return 16383;
    }

    int off = v - centre;
    if (off >= -PITCH_CENTER_DEADZONE && off <= PITCH_CENTER_DEADZONE) {
        return mid14;
    }

    // Two straight lines meeting at the detent rather than one across the whole
    // travel. The detent is not at the electrical midpoint, so a single linear
    // map puts 0 % slightly off centre — which is exactly what it did.
    if (v < centre) {
        return (uint16_t)(((int32_t)(v - lo) * mid14) / (centre - lo));
    }
    return (uint16_t)(mid14 + ((int32_t)(v - centre) * (16383 - mid14)) / (hi - centre));
}

uint16_t pitch_raw(void)            { return s_value; }
uint16_t pitch_centre_tap_raw(void) { return s_centre_tap; }
uint16_t pitch_seen_min(void)       { return s_min; }
uint16_t pitch_seen_max(void)       { return s_max; }
