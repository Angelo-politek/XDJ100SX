#ifndef XDJ_USB_DEV_H
#define XDJ_USB_DEV_H

#include <stdbool.h>

// Composite USB device: a MIDI interface for Mixxx plus a CDC interface that
// carries the debug log, on the same cable. Replaces pico_stdio_usb, which can
// only do CDC — see usb_dev.c for why the two cannot coexist.

// Starts TinyUSB and routes printf() to the CDC interface.
void usb_dev_init(void);

// Services the USB stack. Call it as often as the main loop allows.
void usb_dev_task(void);

// True once a terminal has actually opened the CDC port (DTR asserted).
bool usb_dev_serial_connected(void);

#endif // XDJ_USB_DEV_H
