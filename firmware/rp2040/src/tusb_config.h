#ifndef _TUSB_CONFIG_H_
#define _TUSB_CONFIG_H_

// TinyUSB configuration for the composite device: one USB-MIDI interface for
// Mixxx plus one CDC interface for the debug log, on the same cable.
//
// This file is only picked up because the project links tinyusb_device
// directly instead of pico_stdio_usb. pico_stdio_usb ships its own
// CDC-only tusb_config.h, which would shadow this one — see
// usb_dev.c for the rest of the reasoning.

#ifndef CFG_TUSB_MCU
#define CFG_TUSB_MCU            OPT_MCU_RP2040
#endif
#ifndef CFG_TUSB_OS
#define CFG_TUSB_OS             OPT_OS_PICO
#endif

#define CFG_TUSB_RHPORT0_MODE   OPT_MODE_DEVICE
#define CFG_TUD_ENABLED         1
#define CFG_TUD_MAX_SPEED       OPT_MODE_FULL_SPEED

#define CFG_TUSB_MEM_SECTION
#define CFG_TUSB_MEM_ALIGN      __attribute__ ((aligned(4)))

#define CFG_TUD_ENDPOINT0_SIZE  64

#define CFG_TUD_CDC             1
#define CFG_TUD_MIDI            1
#define CFG_TUD_MSC             0
#define CFG_TUD_HID             0
#define CFG_TUD_VENDOR          0

// The log can burst a dozen lines at once during bring-up, so give the CDC
// transmit FIFO room; the receive side only ever carries single keystrokes.
#define CFG_TUD_CDC_RX_BUFSIZE  64
#define CFG_TUD_CDC_TX_BUFSIZE  512
#define CFG_TUD_CDC_EP_BUFSIZE  64

#define CFG_TUD_MIDI_RX_BUFSIZE 128
#define CFG_TUD_MIDI_TX_BUFSIZE 128
#define CFG_TUD_MIDI_EP_BUFSIZE 64

#endif // _TUSB_CONFIG_H_
