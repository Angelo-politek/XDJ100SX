#include "midi.h"

#include "tusb.h"

#include "board_config.h"

#define MIDI_CABLE 0

// Status bytes. Mixxx's mapping expects the buttons and the pitch on channel 1,
// the jog on channel 2 and the browse encoder on channel 3.
#define STATUS_NOTE_ON_CH1  0x90
#define STATUS_NOTE_OFF_CH1 0x80
#define STATUS_CC_CH1       0xB0
#define STATUS_CC_CH2       0xB1
#define STATUS_NOTE_ON_CH3  0x92
#define STATUS_NOTE_OFF_CH3 0x82

// Deck to Mixxx
#define CC_PITCH_MSB   0x00
#define CC_PITCH_LSB   0x20
#define CC_JOG         0x14
#define JOG_FORWARD    65     // relative CC: 64 is the centre, so +1 / -1
#define JOG_BACKWARD   63
#define NOTE_LOAD      73
#define NOTE_BROWSE_DOWN 70
#define NOTE_BROWSE_UP   71

// Mixxx to deck
#define NOTE_LED_PLAY     61  // play_indicator
#define NOTE_LED_CUE      62  // cue_indicator
#define NOTE_LED_BEAT     63  // beat_active
#define NOTE_LED_EOT      64  // end_of_track
#define NOTE_PLAY_LATCHED 65  // gates the beat flash

#define BEAT_FLASH_MS      60
#define EOT_BLINK_PERIOD_MS 1000

static const uint8_t kButtonNote[BTN_COUNT] = {
    [BTN_PLAY]         = 60,
    [BTN_CUE]          = 61,
    [BTN_MASTER_TEMPO] = 62,
    [BTN_EJECT]        = 63,
    [BTN_TRACK_PREV]   = 64,
    [BTN_TRACK_NEXT]   = 65,
    [BTN_SEARCH_BACK]  = 66,
    [BTN_SEARCH_FWD]   = 67,
    [BTN_JET]          = 68,
    [BTN_ZIP]          = 69,
    [BTN_WAH]          = 70,
    [BTN_HOLD]         = 71,
    [BTN_TIME]         = 72,
};

static int32_t   s_jog_accum;
static int32_t   s_browse_accum;
static int       s_last_msb = -1;
static int       s_last_lsb = -1;
static int       s_last_sent14 = -1;
static bool      s_playing;
static bool      s_was_mounted;
static xdj_led_t s_beat_led = LED_FOR_BEAT;
static xdj_led_t s_eot_led  = LED_FOR_END_OF_TRACK;

static void send3(uint8_t status, uint8_t d1, uint8_t d2) {
    if (!tud_midi_mounted()) {
        return;
    }
    uint8_t msg[3] = { status, d1, d2 };
    tud_midi_stream_write(MIDI_CABLE, msg, sizeof(msg));
}

void midi_init(void) {
    s_jog_accum = 0;
    s_browse_accum = 0;
    s_last_msb = -1;
    s_last_lsb = -1;
    s_last_sent14 = -1;
    s_playing = false;
}

bool midi_ready(void) {
    return tud_midi_mounted();
}

void midi_send_button(xdj_button_t button, bool pressed) {
    if (button >= BTN_COUNT) {
        return;
    }
    send3(pressed ? STATUS_NOTE_ON_CH1 : STATUS_NOTE_OFF_CH1,
          kButtonNote[button], pressed ? 127 : 0);
}

void midi_send_load_button(bool pressed) {
    send3(pressed ? STATUS_NOTE_ON_CH1 : STATUS_NOTE_OFF_CH1,
          NOTE_LOAD, pressed ? 127 : 0);
}

void midi_send_jog_steps(int32_t steps) {
    s_jog_accum += steps;
    while (s_jog_accum >= JOG_STEPS_PER_MIDI_TICK) {
        s_jog_accum -= JOG_STEPS_PER_MIDI_TICK;
        send3(STATUS_CC_CH2, CC_JOG, JOG_FORWARD);
    }
    while (s_jog_accum <= -JOG_STEPS_PER_MIDI_TICK) {
        s_jog_accum += JOG_STEPS_PER_MIDI_TICK;
        send3(STATUS_CC_CH2, CC_JOG, JOG_BACKWARD);
    }
}

void midi_send_browse_steps(int32_t steps) {
    s_browse_accum += steps;
    // Mixxx's browse handler reacts to a Note On; the Note Off keeps the
    // library from treating the entry as held down.
    while (s_browse_accum >= BROWSE_STEPS_PER_MIDI_TICK) {
        s_browse_accum -= BROWSE_STEPS_PER_MIDI_TICK;
        send3(STATUS_NOTE_ON_CH3, NOTE_BROWSE_DOWN, 127);
        send3(STATUS_NOTE_OFF_CH3, NOTE_BROWSE_DOWN, 0);
    }
    while (s_browse_accum <= -BROWSE_STEPS_PER_MIDI_TICK) {
        s_browse_accum += BROWSE_STEPS_PER_MIDI_TICK;
        send3(STATUS_NOTE_ON_CH3, NOTE_BROWSE_UP, 127);
        send3(STATUS_NOTE_OFF_CH3, NOTE_BROWSE_UP, 0);
    }
}

void midi_send_pitch(uint16_t value14) {
    if (value14 > 16383) {
        value14 = 16383;
    }
    // Only move when the fader really moved. s_last_sent14 is kept separate
    // from the MSB/LSB pair because the test has to be on the whole value: an
    // ADC dithering by one count changes the LSB endlessly while the fader is
    // sitting still, and each change would be another CC down the wire.
    if (s_last_sent14 >= 0) {
        int moved = (int)value14 - s_last_sent14;
        if (moved < 0) {
            moved = -moved;
        }
        if (moved < PITCH_MIDI_HYSTERESIS && value14 != 0 && value14 != 16383) {
            return;  // the ends always get through, so 0 % and 100 % are exact
        }
    }
    s_last_sent14 = (int)value14;

    int msb = (value14 >> 7) & 0x7F;
    int lsb = value14 & 0x7F;

    if (msb != s_last_msb) {
        s_last_msb = msb;
        send3(STATUS_CC_CH1, CC_PITCH_MSB, (uint8_t)msb);
    }
    if (lsb != s_last_lsb) {
        s_last_lsb = lsb;
        send3(STATUS_CC_CH1, CC_PITCH_LSB, (uint8_t)lsb);
    }
}

void midi_set_beat_led(xdj_led_t led) {
    if (led >= LED_COUNT) {
        return;
    }
    led_set(s_beat_led, false);
    s_beat_led = led;
}

xdj_led_t midi_beat_led(void) {
    return s_beat_led;
}

static void handle_note(uint8_t note, bool on) {
    switch (note) {
        case NOTE_LED_PLAY:
            led_set(LED_PLAY, on);
            break;
        case NOTE_LED_CUE:
            led_set(LED_CUE, on);
            break;
        case NOTE_LED_BEAT:
            // One note per beat from Mixxx, one short flash here: the beat grid
            // lives in Mixxx, there is nothing to duplicate in the firmware.
            // Gated on play_latched, otherwise it would blink on a stopped deck.
            if (on && s_playing) {
                led_pulse(s_beat_led, BEAT_FLASH_MS);
            }
            break;
        case NOTE_LED_EOT:
            if (on) {
                led_blink(s_eot_led, EOT_BLINK_PERIOD_MS);
            } else {
                led_set(s_eot_led, false);
            }
            break;
        case NOTE_PLAY_LATCHED:
            s_playing = on;
            if (!on) {
                led_set(s_beat_led, false);
            }
            break;
        default:
            break;
    }
}

void midi_task(void) {
    bool mounted = tud_midi_mounted();
    if (mounted && !s_was_mounted) {
        // Mixxx has just opened the port. Forget what we assume it knows, so
        // the next pitch update retransmits the fader's real position instead
        // of leaving the deck's tempo wherever Mixxx last had it.
        s_last_msb = -1;
        s_last_lsb = -1;
        s_last_sent14 = -1;
    }
    s_was_mounted = mounted;

    uint8_t packet[4];
    while (tud_midi_packet_read(packet)) {
        uint8_t status = packet[1];
        uint8_t type = status & 0xF0;
        if (type != 0x90 && type != 0x80) {
            continue;  // only note messages carry LED state
        }
        // A Note On with velocity 0 is the conventional Note Off.
        handle_note(packet[2], (type == 0x90) && (packet[3] > 0));
    }
}
