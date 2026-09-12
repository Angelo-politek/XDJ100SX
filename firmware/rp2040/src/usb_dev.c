#include "usb_dev.h"

#include "pico/stdlib.h"
#include "pico/stdio/driver.h"
#include "pico/bootrom.h"
#include "pico/time.h"
#include "tusb.h"

// Why this file exists instead of pico_enable_stdio_usb():
//
// pico_stdio_usb owns the USB descriptors and ships a tusb_config.h that only
// enables CDC. Linking it would either shadow this project's tusb_config.h or
// clash with its descriptor callbacks, and the MIDI class would never be
// compiled in. Linking tinyusb_device directly puts the whole device in our
// hands — at the cost of re-implementing the two conveniences the SDK provided:
// a stdio driver so printf() still works, and the 1200-baud BOOTSEL reset that
// lets the VS Code Run button reflash without touching the board.

// Writing to a host that has stopped reading must not wedge the firmware.
#define CDC_WRITE_TIMEOUT_MS 100

static void cdc_out_chars(const char *buf, int len) {
    if (!tud_cdc_connected()) {
        return;  // nobody listening: drop it rather than stall the deck
    }

    absolute_time_t deadline = make_timeout_time_ms(CDC_WRITE_TIMEOUT_MS);
    int written = 0;
    while (written < len) {
        uint32_t n = tud_cdc_write(buf + written, (uint32_t)(len - written));
        written += (int)n;
        tud_cdc_write_flush();
        tud_task();
        if (!tud_cdc_connected() || time_reached(deadline)) {
            return;
        }
    }
}

static void cdc_out_flush(void) {
    tud_cdc_write_flush();
}

static int cdc_in_chars(char *buf, int len) {
    if (!tud_cdc_connected() || tud_cdc_available() == 0) {
        return PICO_ERROR_NO_DATA;
    }
    uint32_t n = tud_cdc_read(buf, (uint32_t)len);
    return (n > 0) ? (int)n : PICO_ERROR_NO_DATA;
}

static stdio_driver_t s_cdc_stdio_driver = {
    .out_chars = cdc_out_chars,
    .out_flush = cdc_out_flush,
    .in_chars  = cdc_in_chars,
#if PICO_STDIO_ENABLE_CRLF_SUPPORT
    .crlf_enabled = PICO_STDIO_DEFAULT_CRLF,
#endif
};

// The RP2040 has a single USB controller, so the device root hub port is 0.
// (BOARD_TUD_RHPORT would come from TinyUSB's board BSP, which this project
// does not use.)
#define USB_DEVICE_RHPORT 0

void usb_dev_init(void) {
    tud_init(USB_DEVICE_RHPORT);
    stdio_set_driver_enabled(&s_cdc_stdio_driver, true);
}

void usb_dev_task(void) {
    tud_task();
}

bool usb_dev_serial_connected(void) {
    return tud_cdc_connected();
}

// Opening the port at 1200 baud is the convention picotool and the VS Code
// extension use to ask the board to jump into the bootloader.
void tud_cdc_line_coding_cb(uint8_t itf, cdc_line_coding_t const *coding) {
    (void)itf;
    if (coding->bit_rate == 1200) {
        reset_usb_boot(0, 0);
    }
}
