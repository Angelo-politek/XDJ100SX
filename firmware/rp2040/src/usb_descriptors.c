#include <string.h>

#include "pico/unique_id.h"
#include "tusb.h"

// USB descriptors for the composite device: CDC (debug log) + MIDI (Mixxx).

// Raspberry Pi's vendor ID with a product ID picked for this project. It must
// differ from the SDK's plain-CDC 0x000A: Windows caches a device's descriptors
// per VID/PID, so reusing it on a board that has already enumerated as
// CDC-only leaves the host convinced there is no MIDI interface.
#define USB_VID 0x2E8A
#define USB_PID 0xCD01

enum {
    STRID_LANGID = 0,
    STRID_MANUFACTURER,
    STRID_PRODUCT,
    STRID_SERIAL,
    STRID_CDC,
    STRID_MIDI,
    STRID_MIDI_JACK,
};

enum {
    ITF_NUM_CDC = 0,
    ITF_NUM_CDC_DATA,
    ITF_NUM_MIDI,
    ITF_NUM_MIDI_STREAMING,
    ITF_NUM_TOTAL,
};

#define EPNUM_CDC_NOTIF 0x81
#define EPNUM_CDC_OUT   0x02
#define EPNUM_CDC_IN    0x82
#define EPNUM_MIDI_OUT  0x03
#define EPNUM_MIDI_IN   0x83

// --- Device ---------------------------------------------------------------

static const tusb_desc_device_t desc_device = {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0200,

    // A composite device whose first function is CDC has to declare itself as
    // Miscellaneous/IAD, otherwise the host binds the whole device to the CDC
    // driver and never sees the MIDI interface.
    .bDeviceClass       = TUSB_CLASS_MISC,
    .bDeviceSubClass    = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol    = MISC_PROTOCOL_IAD,

    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor           = USB_VID,
    .idProduct          = USB_PID,
    .bcdDevice          = 0x0100,
    .iManufacturer      = STRID_MANUFACTURER,
    .iProduct           = STRID_PRODUCT,
    .iSerialNumber      = STRID_SERIAL,
    .bNumConfigurations = 1,
};

const uint8_t *tud_descriptor_device_cb(void) {
    return (const uint8_t *)&desc_device;
}

// --- Configuration --------------------------------------------------------

#define CONFIG_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_CDC_DESC_LEN + TUD_MIDI_DESC_LEN)

static const uint8_t desc_configuration[] = {
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, 0x00, 100),
    TUD_CDC_DESCRIPTOR(ITF_NUM_CDC, STRID_CDC, EPNUM_CDC_NOTIF, 8,
                       EPNUM_CDC_OUT, EPNUM_CDC_IN, 64),
    // Hand-expanded TUD_MIDI_DESCRIPTOR: the macro hard-codes the MIDI jacks'
    // string index to 0, and that string is what decides the port name. The
    // ALSA usb-audio driver builds it as "<card name> <jack name>", so naming
    // the jack "Port 1" yields "XDJ100SX Port 1" — byte for byte what the
    // Teensy produced, and therefore the key Mixxx already has a mapping for.
    // Same length as the macro, so TUD_MIDI_DESC_LEN still applies.
    TUD_MIDI_DESC_HEAD(ITF_NUM_MIDI, STRID_MIDI, 1),
    TUD_MIDI_DESC_JACK_DESC(1, STRID_MIDI_JACK),
    TUD_MIDI_DESC_EP(EPNUM_MIDI_OUT, 64, 1),
    TUD_MIDI_JACKID_IN_EMB(1),
    TUD_MIDI_DESC_EP(EPNUM_MIDI_IN, 64, 1),
    TUD_MIDI_JACKID_OUT_EMB(1),
};

const uint8_t *tud_descriptor_configuration_cb(uint8_t index) {
    (void)index;
    return desc_configuration;
}

// --- Strings --------------------------------------------------------------

// The product string becomes the ALSA card name, and Mixxx derives its settings
// key from that. The Teensy build announced itself as plain "XDJ100SX", so the
// mapping saved in the project's Raspberry Pi image is bound to that name —
// keeping it identical makes this firmware a drop-in replacement, with the
// controller already mapped and enabled. Do not "improve" it.
static const char *const string_desc_arr[] = {
    [STRID_LANGID]       = (const char[]){ 0x09, 0x04 },  // English (US)
    [STRID_MANUFACTURER] = "XDJ100SX",
    [STRID_PRODUCT]      = "XDJ100SX",
    [STRID_SERIAL]       = NULL,                          // filled from the board ID
    [STRID_CDC]          = "XDJ100SX Debug Serial",
    [STRID_MIDI]         = "XDJ100SX MIDI",
    [STRID_MIDI_JACK]    = "Port 1",
};

static uint16_t desc_str[32];

const uint16_t *tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
    (void)langid;

    size_t chr_count = 0;

    if (index == STRID_LANGID) {
        memcpy(&desc_str[1], string_desc_arr[STRID_LANGID], 2);
        chr_count = 1;
    } else if (index == STRID_SERIAL) {
        // The unique flash ID keeps two decks apart on the same host.
        char serial[PICO_UNIQUE_BOARD_ID_SIZE_BYTES * 2 + 1];
        pico_get_unique_board_id_string(serial, sizeof(serial));
        chr_count = strlen(serial);
        for (size_t i = 0; i < chr_count; i++) {
            desc_str[1 + i] = (uint16_t)serial[i];
        }
    } else {
        if (index >= TU_ARRAY_SIZE(string_desc_arr) || string_desc_arr[index] == NULL) {
            return NULL;
        }
        const char *str = string_desc_arr[index];
        chr_count = strlen(str);
        const size_t max_count = TU_ARRAY_SIZE(desc_str) - 1;
        if (chr_count > max_count) {
            chr_count = max_count;
        }
        for (size_t i = 0; i < chr_count; i++) {
            desc_str[1 + i] = (uint16_t)str[i];
        }
    }

    desc_str[0] = (uint16_t)((TUSB_DESC_STRING << 8) | (2 * chr_count + 2));
    return desc_str;
}
