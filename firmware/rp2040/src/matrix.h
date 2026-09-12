#ifndef XDJ_MATRIX_H
#define XDJ_MATRIX_H

#include <stdbool.h>
#include <stdint.h>

// The 13 CDJ-100S front panel buttons, decoded from the 5x3 scanned matrix on
// the DISPLAY BOARD. Order matches the schematic reading left to right, top to
// bottom; see the matrix table in docs/rp2040-mod/01-hardware.md.
typedef enum {
    BTN_HOLD = 0,      // S615, DIGITAL JOG BREAK (HOLD)
    BTN_TIME,          // S613, TIME MODE / AUTO CUE
    BTN_EJECT,         // S611
    BTN_MASTER_TEMPO,  // S608
    BTN_TRACK_PREV,    // S601, TRACK SEARCH backwards
    BTN_TRACK_NEXT,    // S604, TRACK SEARCH forwards
    BTN_JET,           // S607, DIGITAL JOG BREAK (JET)
    BTN_ZIP,           // S609, DIGITAL JOG BREAK (ZIP)
    BTN_WAH,           // S610, DIGITAL JOG BREAK (WAH)
    BTN_PLAY,          // S603, PLAY/PAUSE
    BTN_CUE,           // S606
    BTN_SEARCH_BACK,   // S602, SEARCH backwards
    BTN_SEARCH_FWD,    // S605, SEARCH forwards
    BTN_COUNT
} xdj_button_t;

// Configures the column outputs and row inputs.
void matrix_init(void);

// Runs one full 5-column scan with debouncing. Call about every millisecond;
// MATRIX_DEBOUNCE_SCANS is expressed in units of this call.
void matrix_scan(void);

// Debounced state of one button, true while held.
bool matrix_is_pressed(xdj_button_t button);

// Pops the next press/release since the last call. Returns false when the
// queue is empty. `pressed` is true for a press, false for a release.
bool matrix_pop_event(xdj_button_t *button, bool *pressed);

// Raw (undebounced) bitmap of the last scan: bit (row * MATRIX_COLS + col).
// Only used by the bring-up dump, to see wiring problems the decoder hides.
uint16_t matrix_raw_bitmap(void);

// True when the last scan saw a key combination the matrix cannot resolve
// (three keys forming a rectangle, which ghosts the fourth corner). The
// decoder holds the previous state for the affected keys while this is true.
bool matrix_ghost_detected(void);

// Wiring diagnostic: holds one column permanently high (0..4 = S1..S5) and
// suspends scanning, so the strobe can be measured on the connector with a
// multimeter. Any other value resumes normal scanning. While a column is
// forced, matrix_raw_bitmap() still updates but no press/release events are
// generated.
void matrix_force_column(int col);
int  matrix_forced_column(void);

// Human readable name, e.g. "PLAY". Never NULL for a valid button.
const char *matrix_button_name(xdj_button_t button);

#endif // XDJ_MATRIX_H
