// XDJ100SX / RP2040 fork — panel firmware.
//
// Reads the CDJ-100S front panel through CN601 (buttons, jog wheel, pitch
// fader) plus the added browse encoder, sends it to Mixxx as USB MIDI, and
// drives the panel LEDs from what Mixxx sends back. The same USB cable also
// carries a CDC serial log with the bring-up diagnostics.
//
// Wiring: docs/rp2040-mod/02-wiring.md
// Theory: docs/rp2040-mod/01-hardware.md

#include <stdio.h>

#include "pico/stdlib.h"

#include "board_config.h"
#include "encoder.h"
#include "leds.h"
#include "matrix.h"
#include "midi.h"
#include "pitch.h"
#include "usb_dev.h"

#define SCAN_INTERVAL_US    1000    // matrix + browse switch debounce tick
#define PITCH_INTERVAL_US   5000    // ADC is slower moving, no need to rush
#define ENCODER_REPORT_US   50000   // coalesce encoder steps into one line
#define RAW_DUMP_US         200000  // only while raw mode is on
#define PITCH_REPORT_US     150000  // floor between two PITCH lines
// Hysteresis on the printed value, in 14-bit units. 32 is about 8 ADC
// counts, below what the fader's own mechanical play resolves.
#define PITCH_PRINT_HYSTERESIS 32
#define MIDI_DRAIN_US       2000    // how often encoder deltas become MIDI
#define HEARTBEAT_US        500000

#ifdef CYW43_WL_GPIO_LED_PIN
#include "pico/cyw43_arch.h"
#endif

static bool s_raw_mode;
static bool s_pitch_report = true;

// Simulates the beat notes Mixxx sends, for checking the flash without a host.
// 120 BPM, 60 ms flash.
#define BEAT_SIM_PERIOD_US 500000
#define BEAT_SIM_FLASH_MS  60
static bool s_beat_sim;

static void pico_led_init(void) {
#ifdef CYW43_WL_GPIO_LED_PIN
    cyw43_arch_init();
#else
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
#endif
}

static void pico_led_set(bool on) {
#ifdef CYW43_WL_GPIO_LED_PIN
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, on);
#else
    gpio_put(PICO_DEFAULT_LED_PIN, on);
#endif
}

static void print_banner(void) {
    printf("\n");
    printf("=== XDJ100SX RP2040 ===\n");
    printf("Panel: CDJ-100S DISPLAY BOARD ASSY (DWG1503) via CN601\n");
    printf("Matrix: %d cols (S1..S5) x %d rows (KD0..KD2), active high\n",
           MATRIX_COLS, MATRIX_ROWS);
    printf("Commands: h=help r=raw s=status z=zero p=pitch c=force-col l/1-4=LED b=beat\n");
    printf("USB MIDI: %s\n", midi_ready() ? "connected" : "waiting for host");
    printf("Note: an unwired input floats. The pitch ADC especially reads noise\n");
    printf("      until CN601 pin 2 is connected - press p to mute it.\n");
    printf("Press a button on the deck.\n\n");
}

static void print_help(void) {
    printf("h  this help\n");
    printf("r  toggle the continuous raw dump (matrix bitmap + ADC + encoders)\n");
    printf("s  print a one-shot status line\n");
    printf("z  zero the encoder positions and the pitch min/max calibration\n");
    printf("p  toggle pitch reporting (mute it while the fader is unwired)\n");
    printf("c  cycle the forced-column diagnostic: holds S1..S5 high one at a\n");
    printf("   time so the strobe can be measured on CN601 pins 11..15\n");
    printf("l  LED self test: PLAY, CUE, DISC, WINDOW in turn\n");
    printf("1..4  toggle one LED (1=PLAY 2=CUE 3=DISC 4=WINDOW)\n");
    printf("b  toggle the 120 BPM beat-flash simulation\n");
    printf("B  pick which LED the beat flash uses\n");
}

// One line per matrix row, so a miswired column or a stuck row is obvious at a
// glance instead of having to decode a hex bitmap.
static void print_raw(void) {
    uint16_t bits = matrix_raw_bitmap();
    printf("RAW  ");
    for (int r = 0; r < MATRIX_ROWS; r++) {
        printf("KD%d[", r);
        for (int c = 0; c < MATRIX_COLS; c++) {
            printf("%c", (bits & (1u << (r * MATRIX_COLS + c))) ? 'X' : '.');
        }
        printf("] ");
    }
    printf("| pitch=%4u ct=%4u | jog=%ld browse=%ld sw=%d",
           pitch_raw(), pitch_centre_tap_raw(),
           (long)encoder_position(ENC_JOG),
           (long)encoder_position(ENC_BROWSE),
           browse_switch_pressed() ? 1 : 0);
    if (matrix_forced_column() >= 0) {
        printf("  <FORCED S%d>", matrix_forced_column() + 1);
    }
    printf("%s\n", matrix_ghost_detected() ? "  <GHOST>" : "");
}

static void print_status(void) {
    printf("STATUS  pitch raw=%u midi14=%u seen_min=%u seen_max=%u span=%d  ct=%u\n",
           pitch_raw(), pitch_value14(), pitch_seen_min(), pitch_seen_max(),
           (int)pitch_seen_max() - (int)pitch_seen_min(),
           pitch_centre_tap_raw());
    printf("        jog=%ld  browse=%ld  browse_sw=%s\n",
           (long)encoder_position(ENC_JOG),
           (long)encoder_position(ENC_BROWSE),
           browse_switch_pressed() ? "DOWN" : "up");
    printf("        midi=%s  beat LED=%s\n",
           midi_ready() ? "connected" : "not connected", led_name(midi_beat_led()));
    printf("        held:");
    bool any = false;
    for (int b = 0; b < BTN_COUNT; b++) {
        if (matrix_is_pressed((xdj_button_t)b)) {
            printf(" %s", matrix_button_name((xdj_button_t)b));
            any = true;
        }
    }
    printf("%s\n", any ? "" : " (none)");
}

static void handle_console(void) {
    int ch = getchar_timeout_us(0);
    if (ch == PICO_ERROR_TIMEOUT) {
        return;
    }
    switch (ch) {
        case 'h': print_help(); break;
        case 'r':
            s_raw_mode = !s_raw_mode;
            printf("raw dump %s\n", s_raw_mode ? "ON" : "OFF");
            break;
        case 's': print_status(); break;
        case 'z':
            encoder_take_delta(ENC_JOG);
            encoder_take_delta(ENC_BROWSE);
            pitch_init();
            printf("encoders and pitch calibration reset\n");
            break;
        case 'p':
            s_pitch_report = !s_pitch_report;
            printf("pitch reporting %s\n", s_pitch_report ? "ON" : "OFF");
            break;
        case 'c': {
            // Cycle S1..S5 then back to normal scanning.
            int next = matrix_forced_column() + 1;
            if (next >= MATRIX_COLS) {
                next = -1;
            }
            matrix_force_column(next);
            if (next < 0) {
                printf("column force OFF, scanning again\n");
            } else {
                printf("forcing column S%d high (CN601 pin %d, GP%d) - scanning paused\n",
                       next + 1, 11 + next, kMatrixColPins[next]);
                if (!s_raw_mode) {
                    s_raw_mode = true;
                    printf("raw dump ON\n");
                }
            }
            break;
        }
        case 'l':
            leds_start_self_test();
            printf("LED self test running\n");
            break;
        case 'b':
            s_beat_sim = !s_beat_sim;
            if (!s_beat_sim) {
                led_set(midi_beat_led(), false);
            }
            printf("beat flash simulation %s on %s (120 BPM)\n",
                   s_beat_sim ? "ON" : "OFF", led_name(midi_beat_led()));
            break;
        case 'B':
            midi_set_beat_led((xdj_led_t)((midi_beat_led() + 1) % LED_COUNT));
            printf("beat flash LED is now %s\n", led_name(midi_beat_led()));
            break;
        case '1': case '2': case '3': case '4': {
            xdj_led_t led = (xdj_led_t)(ch - '1');
            bool on = !led_get(led);
            led_set(led, on);
            printf("LED  %-6s %s\n", led_name(led), on ? "ON" : "off");
            break;
        }
        default: break;
    }
}

int main(void) {
    usb_dev_init();
    pico_led_init();

    matrix_init();
    encoder_init();
    pitch_init();
    leds_init();
    midi_init();

    // The banner is printed when a terminal actually opens the CDC port, not at
    // boot: the board is usually already running by the time the serial monitor
    // is opened, and a banner printed into the void looks like a dead firmware.
    bool usb_was_connected = false;

    uint32_t next_scan = time_us_32();
    uint32_t next_pitch = next_scan;
    uint32_t next_encoder = next_scan;
    uint32_t next_raw = next_scan;
    uint32_t next_pitch_print = next_scan;
    uint32_t next_midi = next_scan;
    uint16_t last_print14 = pitch_value14();
    int32_t last_jog_pos = encoder_position(ENC_JOG);
    int32_t last_browse_pos = encoder_position(ENC_BROWSE);
    uint32_t next_beat = next_scan;
    uint32_t next_heartbeat = next_scan;
    bool led_on = false;

    while (true) {
        usb_dev_task();
        midi_task();

        uint32_t now = time_us_32();

        if ((int32_t)(now - next_scan) >= 0) {
            next_scan += SCAN_INTERVAL_US;
            matrix_scan();
            browse_switch_update();
            leds_task();

            xdj_button_t button;
            bool pressed;
            while (matrix_pop_event(&button, &pressed)) {
                midi_send_button(button, pressed);
                printf("BTN  %-13s %s\n", matrix_button_name(button),
                       pressed ? "DOWN" : "up");
            }
            if (browse_switch_take_press()) {
                midi_send_load_button(true);
                midi_send_load_button(false);
                printf("BTN  %-13s DOWN\n", "BROWSE_LOAD");
            }
        }

        if ((int32_t)(now - next_pitch) >= 0) {
            next_pitch += PITCH_INTERVAL_US;
            pitch_update();
            midi_send_pitch(pitch_value14());
        }

        // Compare against what was actually printed, and only push the rate
        // limiter forward when a line really goes out. Testing the limiter
        // first and discarding the change otherwise loses the value the fader
        // comes to rest on — which is the one that matters.
        if (s_pitch_report && (int32_t)(now - next_pitch_print) >= 0) {
            uint16_t v14 = pitch_value14();
            int moved = (int)v14 - (int)last_print14;
            if (moved < 0) {
                moved = -moved;
            }
            if (moved >= PITCH_PRINT_HYSTERESIS) {
                last_print14 = v14;
                next_pitch_print = now + PITCH_REPORT_US;
                printf("PITCH raw=%4u midi14=%5u  (seen %u..%u)\n",
                       pitch_raw(), v14, pitch_seen_min(), pitch_seen_max());
            }
        }

        // The MIDI layer owns encoder_take_delta(): draining it here, fast and
        // in one place, keeps the detent accounting exact. The log works from
        // absolute positions instead, so the two never steal steps from each
        // other.
        if ((int32_t)(now - next_midi) >= 0) {
            next_midi += MIDI_DRAIN_US;
            midi_send_jog_steps(encoder_take_delta(ENC_JOG));
            midi_send_browse_steps(encoder_take_delta(ENC_BROWSE));
        }

        if ((int32_t)(now - next_encoder) >= 0) {
            next_encoder += ENCODER_REPORT_US;
            int32_t jog_pos = encoder_position(ENC_JOG);
            int32_t browse_pos = encoder_position(ENC_BROWSE);
            if (jog_pos != last_jog_pos) {
                printf("JOG  %+ld  (pos %ld)\n",
                       (long)(jog_pos - last_jog_pos), (long)jog_pos);
                last_jog_pos = jog_pos;
            }
            if (browse_pos != last_browse_pos) {
                printf("BRWS %+ld  (pos %ld)\n",
                       (long)(browse_pos - last_browse_pos), (long)browse_pos);
                last_browse_pos = browse_pos;
            }
        }

        if (s_beat_sim && (int32_t)(now - next_beat) >= 0) {
            next_beat += BEAT_SIM_PERIOD_US;
            led_pulse(midi_beat_led(), BEAT_SIM_FLASH_MS);
        }

        if (s_raw_mode && (int32_t)(now - next_raw) >= 0) {
            next_raw += RAW_DUMP_US;
            print_raw();
        }

        if ((int32_t)(now - next_heartbeat) >= 0) {
            next_heartbeat += HEARTBEAT_US;
            led_on = !led_on;
            pico_led_set(led_on);

            bool usb_connected = usb_dev_serial_connected();
            if (usb_connected && !usb_was_connected) {
                print_banner();
            }
            usb_was_connected = usb_connected;
        }

        handle_console();
    }
}
