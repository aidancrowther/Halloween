# Eight-channel 12 V PWM DMX fixture

## Fixture personality

This firmware controls eight independent low-side MOSFET outputs for 12 V
single-color LED loads. It occupies **8 consecutive DMX slots**, one intensity
channel per output. DMX values map directly to 8-bit SoftPWM duty values (0 is
off; 255 is full on). All eight levels from each received fixture frame are
applied together.

| Fixture-relative slot | Output | ATmega8 pin |
| ---: | ---: | --- |
| 1 | PWM output 1 | PD7 |
| 2 | PWM output 2 | PD6 |
| 3 | PWM output 3 | PD5 |
| 4 | PWM output 4 | PD4 |
| 5 | PWM output 5 | PD3 |
| 6 | PWM output 6 | PD2 |
| 7 | PWM output 7 | PC4 |
| 8 | PWM output 8 | PC3 |

The start-address DIP switch follows the WS2812 variant: active-low with
internal pull-ups and reversed bit wiring. The firmware computes
`start_address = 1 + switch_value`; switch bits map PB1..PB5 to address bits
8..4 and PC0..PC2 to bits 3..1. Valid start addresses are **1–505**.

## DMX reception and fail-safe

The ATmega8 USART receives 250 kbaud, 8N1. UART framing error identifies the
DMX break; only start code zero is accepted. UART overrun/parity errors reject
the current frame. The MAX485 is permanently wired for receive, so the firmware
does not control DE or `/RE`; its receiver output (RO) must connect to RXD/PD0.

## Status LED

The same single RGB WS2812 status pixel used by the WS2812 firmware is connected
directly to **PC5**. It displays dim red when no valid fixture DMX has been
received for one second (or the address is invalid), and green after a complete
valid 8-slot fixture footprint is received. The LED is written only when this
status changes: a green update is queued on the fixture's last slot and sent
after the receiver reaches the universe end; if it overlaps trailing slots, the
parser resynchronizes at the next break. A red timeout update is sent after one
second without a complete fixture update. This is a coarse activity indicator,
not a precision DMX monitor.

All eight DMX slots are staged and copied as a frame before outputs are updated.
If no complete fixture data is received for 1 second, or the selected address
is invalid, all eight outputs are set to zero (blackout). Power-up also starts
with every output off.

## PWM implementation

The firmware uses the installed **arduino-softpwm 1.0.1** (Palatis SoftPWM)
library. Eight compile-time channels map to the MOSFET gate pins above. The
library uses Timer1; do not combine this mode with other Timer1 users. PWM is
configured for 256 levels to preserve the full DMX resolution, at a requested
100 Hz PWM frequency. This software PWM has higher interrupt load than the
64-level ramp test, so check MOSFET gate waveforms and controller stability on
the actual board/load before deployment.

## Exclusive board mode and build

The board jumper routes the shared MCU pins either to WS2812 data or MOSFET
gates. Use this firmware only in **12 V PWM mode**; it is mutually exclusive
with the WS2812 firmware. Use an external 12 V supply within the LED and MOSFET
current/thermal ratings, with the board's required common ground.

Build target: **MiniCore ATmega8**, 16 MHz external clock, UART bootloader:

```text
MiniCore:avr:8:clock=16MHz_external,bootloader=uart0
```