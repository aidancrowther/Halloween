# Eight-segment WS2812 DMX fixture

## Fixture proposal and personality

The ATmega8 controls eight independent WS2812 strings in parallel. Each string
contains seven RGB pixels. The fixture occupies **24 consecutive DMX slots**;
each segment gets three 8-bit intensity channels, with no master dimmer or mode
channel. This is simple to patch and allows all eight segments to be controlled
independently.

| Fixture-relative slot | Function |
| ---: | --- |
| 1–3 | Segment 1: Red, Green, Blue |
| 4–6 | Segment 2: Red, Green, Blue |
| 7–9 | Segment 3: Red, Green, Blue |
| 10–12 | Segment 4: Red, Green, Blue |
| 13–15 | Segment 5: Red, Green, Blue |
| 16–18 | Segment 6: Red, Green, Blue |
| 19–21 | Segment 7: Red, Green, Blue |
| 22–24 | Segment 8: Red, Green, Blue |

Every pixel in a segment receives that segment's color. DMX is RGB at the
controller-facing level; the firmware reorders each pixel to **GRB** on the
wire, matching the reference Bird Skeleton fixture and its RGB (three-channel,
not RGBW) pixels. Values are passed through at full 0–255 intensity.

The footprint is valid at start addresses **1–489**. The fixture updates after
receiving all 24 slots from a valid DMX frame. If DMX stops or the address is
invalid, the last color is held; power-up output is black.

## Address selection and receiver assumptions

The start address follows the reference project's active-low, pull-up DIP-switch
decoding. The logical address is `1 + switch_value`; switch bit wiring/order is:

| Address bit | ATmega8 pin |
| ---: | --- |
| 8–4 | PB1, PB2, PB3, PB4, PB5 |
| 3–1 | PC0, PC1, PC2 |

The selected DIP pin reads low when its switch is ON. Firmware waits 10 ms after
enabling the pull-ups, then samples the inputs nine times at 2 ms intervals and
uses a majority vote for each bit. This filters brief startup/contact noise; it
cannot correct incorrect PCB wiring or a continuously unstable input. Pin
mappings should be confirmed against the actual PCB schematic before installation.

DMX reception uses the ATmega8 USART at 250 kbaud, 8N1. As in the reference,
UART framing error is used to detect the DMX break and only start code zero is
accepted. The MAX485 is permanently wired for receive, so this sketch does not
need to switch its DE or `/RE` pins. Unlike the reference project, PD7 cannot be
used as a DMX mode output because PD7 is segment 1's WS2812 data output here.
The MAX485 receiver output must connect to RXD/PD0.

## Status LED

A single RGB WS2812 status pixel is connected directly to **PC5**. It shows dim
red when no valid fixture DMX has been received in the last second or the start
address is invalid, and green after a valid fixture footprint is received. Its
color is rewritten only on a status change. A green update is queued when the
fixture's final slot arrives and sent after the receiver reaches the universe
end; if it overlaps trailing slots, the parser resynchronizes at the next break.
A red timeout update is sent after one second without a complete fixture update.
The LED is a coarse activity indicator, not a precision DMX monitor.

At startup, before the DMX UART receiver is enabled, the status pixel counts out
the DIP-selected start address in decimal digits: long flashes (700 ms) count
hundreds, medium flashes (350 ms) count tens, and short flashes (120 ms) count
ones. A zero-valued digit is shown as an empty group pause. For example, address
241 is two long, four medium, and one short flash. The normal red/green status
indication begins after this sequence.

## LED outputs

| Segment | Data output |
| ---: | --- |
| 1 | PC3 |
| 2 | PC4 |
| 3 | PD2 |
| 4 | PD3 |
| 5 | PD4 |
| 6 | PD5 |
| 7 | PD6 |
| 8 | PD7 |

This group-to-pin order matches the 12 V PWM variant's output order, so Light 1
(Group 1) uses the same MCU pin in either board mode.

The bit timing is emitted with clock-counted AVR `OUT` instructions and expects
a real 16 MHz clock. To avoid blocking DMX for a full eight-string refresh, the
firmware sends **one segment per received fixture update**, round-robin across
segments. Each update shifts seven RGB pixels (about 210 microseconds) and then
latches that segment for 80 microseconds (about 290 microseconds total). Other segments keep their
previously latched colors. Interrupts are disabled during the shift, so a few
DMX bytes may be lost; the parser resynchronizes at the next break and retries
using subsequent frames. A full sweep takes eight valid DMX updates. Use a
shared ground and a suitably rated external supply for 56 LEDs; do not power
them from the Uno/ATmega8 regulator.

## Build and upload

The sketch supports both the original ATmega8 and ATmega328P. For the known-good
ATmega328P on an Uno-compatible serial bootloader, select **Arduino Uno** (16 MHz
external clock). Example CLI FQBN:

```text
arduino:avr:uno
```

For the original ATmega8, select **MiniCore ATmega8**, **16 MHz external clock**,
and the installed UART bootloader (`MiniCore:avr:8:clock=16MHz_external,bootloader=uart0`).
The sketch is `/WS2812.ino` in this directory; no external libraries are used.