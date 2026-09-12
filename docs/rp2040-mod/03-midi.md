# MIDI protocol

Identical to the Teensy firmware's, so [`mixxx/MIDI/`](../../mixxx/MIDI/) works
unchanged. Extracted from `arduino/XDJ100SX.ino` and verified line by line
against `mixxx/MIDI/XDJ100SX.midi.xml`, which is the authoritative source —
the table on p.16 of the project PDF is an older revision.

## Deck → Mixxx

### Buttons — Note On/Off, channel 1

Note On velocity 127 on press, Note Off velocity 0 on release.

| Note | dec | Button | Mixxx control |
|---|---|---|---|
| 0x3C | 60 | PLAY/PAUSE | `play` |
| 0x3D | 61 | CUE | `XDJ100SX.cue` |
| 0x3E | 62 | MASTER TEMPO | `XDJ100SX.key` |
| 0x3F | 63 | EJECT | `XDJ100SX.backButton` |
| 0x40 | 64 | TRACK ◀◀ | `loop_halve` |
| 0x41 | 65 | TRACK ▶▶ | `loop_double` |
| 0x42 | 66 | SEARCH ◀◀ | `XDJ100SX.searchButton` |
| 0x43 | 67 | SEARCH ▶▶ | `XDJ100SX.searchButton` |
| 0x44 | 68 | JET | `XDJ100SX.button1` |
| 0x45 | 69 | ZIP | `XDJ100SX.button2` |
| 0x46 | 70 | WAH | `XDJ100SX.button3` |
| 0x47 | 71 | HOLD | `XDJ100SX.shift` |
| 0x48 | 72 | TIME/AUTO CUE | `XDJ100SX.buttonMode` |
| 0x49 | 73 | LOAD (browse push) | `XDJ100SX.loadTrack` |

### Jog — Control Change, channel 2

CC `0x14` (20), relative: **65** forward, **63** back, one message per detent
(every 4 quadrature steps). Mixxx control `XDJ100SX.nudgeWheelTurn`.

### Browse encoder — Note On+Off, channel 3

Note `0x46` (70) clockwise, `0x47` (71) counter-clockwise; one On+Off pair per
detent. Controls `XDJ100SX.browseDown` / `browseUp`.

### Pitch — 14-bit Control Change, channel 1

CC `0x00` MSB, CC `0x20` (32) LSB, control `XDJ100SX.pitch`. Only the byte that
changed is sent.

The ADC is 12-bit, so one count is about four 14-bit steps, and the RP2040's
ADC dithers by a count even with the fader still. Without hysteresis on the
**value actually sent**, the deck streams a CC forever. `PITCH_MIDI_HYSTERESIS`
handles that; the two extremes are always let through so 0 % and 100 % stay
exact.

## Mixxx → Deck (LEDs)

Note On, channel 1. Velocity > 0 on, 0 off.

| Note | dec | Mixxx control | LED |
|---|---|---|---|
| 0x3D | 61 | `play_indicator` | D614 `[PLAY]` |
| 0x3E | 62 | `cue_indicator` | D616 `[CUE]` |
| 0x3F | 63 | `beat_active` | D620 `[WINDOW]` — beat flash |
| 0x40 | 64 | `end_of_track` | D618 `[DISC IND]` — end-of-track blink |
| 0x41 | 65 | `play_latched` | no LED: gates the beat flash |

- **beat_active**: one note per beat from Mixxx, one ~60 ms `led_pulse()` here.
  The beat grid lives in Mixxx; there is nothing to duplicate in firmware.
  Gated on `play_latched`, otherwise it blinks on a stopped deck.
- **end_of_track**: velocity > 0 starts a ~1 Hz blink, 0 stops it.

Which LED shows what is set by `LED_FOR_BEAT` and `LED_FOR_END_OF_TRACK` in
`board_config.h`, or at runtime with the `B` console command.

Numbers that look like collisions but are not, because they differ in direction
or channel: note 61 is CUE outbound and `play_indicator` inbound; note 62 is
MASTER TEMPO outbound and `cue_indicator` inbound; notes 70/71 are WAH/HOLD on
channel 1 and browse on channel 3.

## USB implementation

A **composite device**: one USB-MIDI interface plus a CDC interface carrying
the debug log, on the same cable. The log therefore stays readable while Mixxx
holds the MIDI port — which the Teensy build could not do.

| File | Role |
|---|---|
| `tusb_config.h` | enables TinyUSB's CDC and MIDI classes |
| `usb_descriptors.c` | the composite descriptors |
| `usb_dev.c` | TinyUSB start-up, stdio driver over CDC, 1200-baud BOOTSEL reset |
| `midi.c` | the protocol above |

### Why not `pico_stdio_usb`

The SDK library that normally provides `printf()` over USB is **CDC only**: it
owns the device descriptors and ships a `tusb_config.h` that never enables the
MIDI class. Keeping it means either its config shadows this project's or its
descriptor callbacks collide with ours.

So the project links `tinyusb_device` directly and re-implements the two
conveniences the SDK gave: a stdio driver so `printf()` still works, and the
BOOTSEL reset when the host opens the port at 1200 baud — which is what lets
the VS Code *Run* button reflash without touching the board.

### The device must be called `XDJ100SX`, and the jack `Port 1`

This one costs an evening if you get it wrong, because nothing reports an error.

On Linux the USB **product string** becomes the ALSA card name, and the
usb-audio driver builds the port name as **card name + MIDI jack name**. Mixxx
derives its settings key from that. The Teensy announced itself as `XDJ100SX`
with a jack called `Port 1`, giving the port `XDJ100SX Port 1` and the key
`XDJ100SX_Port_1` — which is what the project's Raspberry Pi image already has
saved and enabled.

TinyUSB's `TUD_MIDI_DESCRIPTOR` hard-codes the jacks' string index to `0`,
which yields `XDJ100SX MIDI 1` instead. So `usb_descriptors.c` expands that
macro by hand and passes a string index to `TUD_MIDI_DESC_JACK_DESC`.

Get it right and the controller comes up already mapped and enabled, with no
trip into Mixxx's preferences. Get it wrong and Mixxx finds the device, says
`Controller polling stopped`, and silently ignores it.

### VID and PID

`0x2E8A` (Raspberry Pi) with PID `0xCD01`. It **must** differ from the SDK's
plain-CDC `0x000A`: Windows caches descriptors per VID/PID, so reusing it on a
board that already enumerated as CDC-only leaves the host convinced there is no
MIDI interface. The serial number is the Pico's unique flash ID, so two decks on
one host stay distinct.

### Other implementation notes

- The MIDI layer is the only caller of `encoder_take_delta()`, draining it every
  2 ms. The serial log works from absolute positions instead, so the two never
  steal steps from each other.
- `JOG_STEPS_PER_MIDI_TICK` and `BROWSE_STEPS_PER_MIDI_TICK` are 4, i.e. one
  message per detent, matching the Teensy. Dropping the jog value to 1
  quadruples its resolution.
- When Mixxx opens the port the firmware forgets the last MSB/LSB it sent and
  retransmits the fader's real position, rather than leaving the deck's tempo
  wherever Mixxx last had it.
