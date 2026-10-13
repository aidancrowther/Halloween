# Bird Skeleton Animatronic DMX Fixture

Hardware and firmware configuration notes for the ATMega8-based DMX fixture.

## System overview

The fixture receives DMX512 data and uses the selected DMX start address to control:

- Three SG90 servos for animatronic movement.
- A ring of seven WS2812 addressable LEDs.
- A DFPlayer Mini audio module.

The controller is an **ATMega8**. The I/O assignments below are the authoritative pin map for the fixture.

## MCU pin assignment

| ATMega8 pin/port | Function | Electrical/configuration notes |
|---|---|---|
| `PD2` | Servo 1 signal | SG90 control signal; servo power must not come from the MCU I/O pin. |
| `PD3` | Servo 2 signal | SG90 control signal; servo power must not come from the MCU I/O pin. |
| `PD4` | Servo 3 signal | SG90 control signal; servo power must not come from the MCU I/O pin. |
| `PD5` | WS2812 data | Data input to the first LED in the seven-LED chain. |
| `PD6` | `TXOnlySerial` TX to DFPlayer Mini | Transmit-only output connected to the DFPlayer `RX` pin. |
| `PD7` | DMXSerial receive-only mode pin | Dummy output required by the installed `DMXSerial` API; DMX hardware is permanently wired for receive-only operation. |
| `PB1` | DMX address bit 8 | Internal pull-up enabled; DIP switch selects the bit by pulling the pin low. |
| `PB2` | DMX address bit 7 | Internal pull-up enabled; DIP switch selects the bit by pulling the pin low. |
| `PB3` | DMX address bit 6 | Internal pull-up enabled; DIP switch selects the bit by pulling the pin low. |
| `PB4` | DMX address bit 5 | Internal pull-up enabled; DIP switch selects the bit by pulling the pin low. |
| `PB5` | DMX address bit 4 | Internal pull-up enabled; DIP switch selects the bit by pulling the pin low. |
| `PC0` | DMX address bit 3 | Internal pull-up enabled; DIP switch selects the bit by pulling the pin low. |
| `PC1` | DMX address bit 2 | Internal pull-up enabled; DIP switch selects the bit by pulling the pin low. |
| `PC2` | DMX address bit 1 | Internal pull-up enabled; DIP switch selects the bit by pulling the pin low. |

Pins not listed above are reserved until the firmware and schematic assign them.

## DMX address selection

The fixture uses an eight-position DIP switch for address bits 1 through 8. The least significant address bit, bit 0, is permanently set to `1` and is not controlled by a switch.

| Address bit | Input | Switch state for a selected bit | Contribution when selected |
|---:|---|---|---:|
| 0 | Fixed | Always `1` | `1` |
| 1 | `PC2` | Switch ON / pin pulled low | `2` |
| 2 | `PC1` | Switch ON / pin pulled low | `4` |
| 3 | `PC0` | Switch ON / pin pulled low | `8` |
| 4 | `PB5` | Switch ON / pin pulled low | `16` |
| 5 | `PB4` | Switch ON / pin pulled low | `32` |
| 6 | `PB3` | Switch ON / pin pulled low | `64` |
| 7 | `PB2` | Switch ON / pin pulled low | `128` |
| 8 | `PB1` | Switch ON / pin pulled low | `256` |

Because the inputs use internal pull-ups, the logic is active-low:

- Switch open: MCU reads logic `1`; that address bit is **not selected**.
- Switch closed: MCU reads logic `0`; that address bit **is selected**.

The decoded address is:

```text
address = 1
        + (PC2_selected ?   2 : 0)
        + (PC1_selected ?   4 : 0)
        + (PC0_selected ?   8 : 0)
        + (PB5_selected ?  16 : 0)
        + (PB4_selected ?  32 : 0)
        + (PB3_selected ?  64 : 0)
        + (PB2_selected ? 128 : 0)
        + (PB1_selected ? 256 : 0)
```

## DMX personality

The fixture uses a 13-channel DMX footprint beginning at the start address selected
by the DIP switch. The controller should continuously transmit frames through the
fixture's final channel (`start address + 12`). The firmware declares a DMX timeout
of 1000 ms; if no valid packet is received for longer than that, the fixture parks
the servos and clears the LEDs.

For the bench configuration used by `firmware/tests/MegaDmxComponentTest`, the
fixture start address is `289`, so the personality occupies channels `289–301`.
The same offsets apply at any valid configured start address.

| Offset | Bench channel | Function | Values |
|---:|---:|---|---|
| 0 | 289 | Mode | `0–255`; `0–15` is static positioning, higher ranges select animated effects. |
| 1 | 290 | Speed | `0–255`; affects animation phase and servo movement speed. |
| 2 | 291 | Left wing | `0–255`, mapped between the configured left-wing minimum and maximum angles. |
| 3 | 292 | Right wing | `0–255`, mapped between the configured right-wing minimum and maximum angles. |
| 4 | 293 | Beak | `128–255`, mapped from fully closed/resting to fully open; values below `128` clamp closed. |
| 5 | 294 | LED dimmer | `0–255`; scales RGB output. |
| 6 | 295 | Red | `0–255`. |
| 7 | 296 | Green | `0–255`. |
| 8 | 297 | Blue | `0–255`. |
| 9 | 298 | LED effect | `0–255`; selects the LED effect range. |
| 10 | 299 | Effect rate | `0–255`; controls effect timing or intensity where applicable. |
| 11 | 300 | Sound/track | `1–200` requests the corresponding DFPlayer track. Values `240`, `241`, and `242` request DFPlayer commands `0x16`, `0x0E`, and `0x0D`; values `245–250` select the configured named-track aliases. |
| 12 | 301 | Volume | `0–255`, linearly mapped to DFPlayer volume `0–30`. |

### Minimal reception checks

For a static servo test, continuously transmit a frame with mode `0`, speed `0`,
and a changing value in one of the servo channels. For example, set the left-wing
channel to `255` while leaving the other channels at zero. With the current
production calibration, that requests the left wing's configured maximum angle.

For an LED test, set mode `0`, LED dimmer to `255`, and one RGB channel to `255`:

```text
Mode = 0
Dimmer = 255
Red = 255
Green = 0
Blue = 0
```

For an audio test using a root-level `/0001.mp3` file, first transmit sound `0`,
then change the sound channel to `1` while keeping a nonzero volume, such as
volume channel `170` (approximately DFPlayer volume `20`). Audio requests are
edge-triggered: the firmware only requests a new track when the sound-channel
value changes.

The fixture's current production pin map configures the wing ranges as `0°–180°`
and the beak range as `113°–180°`. The wing operating ranges are `8°–172°` to avoid
mechanical stalls at the calibrated servo extremes. In static mode, beak DMX `128` selects the fully
closed/resting position at `113°`, and DMX `255` selects the fully open position at
`180°`; values below `128` clamp closed. When DMX is lost, the wings park at `172°`
and the beak parks at `113°`.

### Mode values

The mode channel selects the movement behavior. The left-wing, right-wing, and
beak channels act as either direct positions or motion amplitudes, depending on
the selected mode. The speed channel controls the phase rate; the exact motion
rate is also limited by the firmware's servo slew step.

| Mode range | Behavior |
|---:|---|
| `0–15` | **Static positioning.** The three servo channels directly select left-wing, right-wing, and beak positions. This is the recommended mode for calibration and basic reception tests. |
| `16–31` | **Synchronized flap, one-shot.** Cool white-blue lighting follows the action; the heavy wing-flap track is requested once. |
| `32–47` | **Alternating wings, one-shot.** Blue-white moving lighting follows the alternating action. |
| `48–63` | **Beak action, one-shot.** Warm amber lighting rises with the beak motion. |
| `64–79` | **Startle, one-shot.** A rapid full-white flash transitions to red while the wings return to idle. |
| `80–95` | **Random twitch, one-shot.** Purple/cold-white flicker accompanies a cached random pose. |
| `96–111` | **Lightning, one-shot.** White flash, blackout, blue-white flash, then fade; the rapid servo step is enforced. |
| `112–127` | **Bird call, one-shot.** Warm amber lighting and repeated beak pulses accompany the call. |
| `128–143` | **Synchronized flap, repeating.** Runs once, returns to idle, turns lighting off, then repeats after a five-second cooldown. |
| `144–159` | **Alternating wings, repeating.** Same five-second post-action cooldown behavior. |
| `160–175` | **Beak action, repeating.** Same five-second post-action cooldown behavior. |
| `176–191` | **Startle, repeating.** Same five-second post-action cooldown behavior. |
| `192–207` | **Random twitch, repeating.** Same five-second post-action cooldown behavior. |
| `208–223` | **Lightning, repeating.** Same five-second post-action cooldown behavior. |
| `224–239` | **Bird call, repeating.** Same five-second post-action cooldown behavior. |
| `240–247` | **Demo, one-shot.** Runs a finite sequence of preset actions, then parks and turns lighting off. |
| `248–251` | **Demo, repeating.** Repeats the demo after a five-second cooldown. |
| `252–255` | **Random preset, repeating.** Selects among the seven action presets, avoids immediate repetition where possible, and repeats after a five-second cooldown. |

In static mode, the LED dimmer, RGB, effect, and effect-rate channels are
interpreted directly. Every non-static preset supplies its own visible lighting
cue and turns the LEDs off after completion. Repeating modes require valid DMX;
after the one-second DMX timeout, active actions and cooldowns are cancelled,
servos park at their minimum positions, and the LEDs are cleared.

### LED effect values

The LED effect channel is divided into effect bands. Each band contains 32
values, allowing the effect-rate channel to vary the selected effect without
changing the broad effect type.

| LED effect range | Behavior |
|---:|---|
| `0–15` | Static RGB output. |
| `16–47` | Breathing/fading intensity. |
| `80–255` | **Static RGB output.** Higher values are accepted but are intentionally rendered deterministically to avoid random per-pixel flicker and preserve stable servo/DMX timing. |
| `240–255` | Warm moving highlight. |

The RGB channels provide the base color. The dimmer channel scales the resulting
color, and the effect-rate channel controls phase, probability, or timing where
the selected effect uses it.

### DFPlayer sound-channel commands

The sound channel is edge-triggered: a command is generated only when its value
changes from the previous received value. Values `1–200` are interpreted as
track numbers and are sent with DFPlayer command `0x03` (play specified track).
For example, sound value `1` requests `/0001.mp3` when that file is arranged as
track 1 on the card.

The following reserved sound values generate direct DFPlayer commands:

| DMX sound value | DFPlayer command | Meaning |
|---:|---:|---|
| `1–200` | `0x03` | Play the specified track number. The DMX value is used as the 16-bit track parameter. |
| `240` | `0x16` | Stop playback. |
| `241` | `0x0E` | Pause playback. |
| `242` | `0x0D` | Play/resume playback. |
| `245` | `0x03` | Play `TRACK_RANDOM_BIRD` → track `1` (`0001.mp3`). |
| `246` | `0x03` | Play `TRACK_LIGHTNING` → track `2` (`0002.mp3`). |
| `247` | `0x03` | Play `TRACK_WING_FLAP` → track `5` (`0005.mp3`). |
| `248` | `0x03` | Play `TRACK_CAGE_RATTLE` → track `9` (`0009.mp3`). |
| `249` | `0x03` | Play `TRACK_CAW` → track `1` (`0001.mp3`). |
| `250` | `0x03` | Play `TRACK_SCREECH` → track `4` (`0004.mp3`). |

The firmware currently transmits DFPlayer commands without requesting feedback
and leaves the DFPlayer `TX` pin disconnected. Volume is handled independently:
the DMX volume value `0–255` is linearly converted to the DFPlayer volume range
`0–30`. To trigger track 1 reliably, transmit sound `0` first, then change it to
`1` while keeping the volume channel nonzero.

The current card assignment is:

| Track | File | Intended use |
|---:|---|---|
| `1` | `0001.mp3` | Primary crow/raven call; random-bird and caw alias. |
| `2` | `0002.mp3` | Ominous/deep call; lightning/startle alias. |
| `3` | `0003.mp3` | Angry/squawking call; direct DMX track selection. |
| `4` | `0004.mp3` | Screech; automatic startle alias. |
| `5` | `0005.mp3` | Heavy wing flap; named wing-flap alias. |
| `6` | `0006.mp3` | Rapid wing flutter; direct DMX track selection. |
| `7` | `0007.mp3` | Flyby/takeoff; direct DMX track selection. |
| `8` | `0008.mp3` | Feather/cloth rustle; direct DMX track selection. |
| `9` | `0009.mp3` | Cage/bone rattle; named cage-rattle alias. |
| `10` | `0010.mp3` | Approximately 45-second ambience; trigger manually with sound value `10`, not automatically from a movement mode. |

In firmware, an input is selected when its raw pin reading is zero. Equivalently, the switch bits can be inverted after reading the ports and then shifted into address positions 1–8.

### Address characteristics

- Minimum address: `1` (all switches open).
- Maximum address: `511` (all switches closed).
- All valid addresses are odd because bit 0 is permanently `1`.
- The fixture therefore supports 256 possible address values, but not the even DMX start addresses.
- The decoded value should be validated before use. A DMX512 start address is normally limited to `1–512`; this mapping can produce `511` but cannot produce `512`.

### Example

To select DMX address `173`:

```text
173 = 1 + 4 + 8 + 32 + 128
```

Close the switches connected to:

- `PC1` (bit 2, value `4`)
- `PC0` (bit 3, value `8`)
- `PB4` (bit 5, value `32`)
- `PB2` (bit 7, value `128`)

Leave the other switches open.

## Final firmware/build configuration

The firmware is an Arduino sketch compiled with `arduino-cli` for the installed **MiniCore 3.0.2 ATmega8** board definition:

- MCU: `ATmega8`.
- Clock: dedicated external `16 MHz` oscillator.
- Bootloader: Arduino bootloader enabled.
- Compiler optimization: MiniCore LTO enabled.
- Build tool: `arduino-cli`; PlatformIO is not used.
- DMX library: the installed `DMXSerial` library, including its ATmega8 support fix.
- DFPlayer library: the installed `TXOnlySerial` library.
- Firmware source: `firmware/BirdSkeletonFixture/`.
- Build output: `firmware/BirdSkeletonFixture/build/MiniCore.avr.8/`.

The firmware is divided into the following modules:

- `DmxInput`: DIP decoding, `DMXSerial` initialization, packet snapshots, and timeout detection.
- `EffectEngine`: servo targets, behavior modes, mode transitions, and effect selection.
- `ServoEngine`: Timer1 servo pulse scheduling and quiet-period reporting.
- `LedEngine`: fixed seven-pixel WS2812 rendering with change detection.
- `AudioEngine`: `TXOnlySerial` packet generation, command queuing, rate limiting, and volume suppression.
- `BirdSkeletonFixture.ino`: initialization and cooperative scheduler.

The DMX transceiver is permanently wired for receive-only operation. The installed `DMXSerial` API still requires a mode-control pin, so the firmware passes `PD7` as a dummy mode pin and holds it in the receive state. `PD7` must not be connected to a transceiver direction input that could enable transmission.

The firmware enables the AVR watchdog with a one-second timeout and resets it from the main scheduler. WS2812 output uses a compact fixed driver for the seven-pixel ring, only transmits when the framebuffer changes, and avoids the flash overhead of a general-purpose pixel library.

Build with:

```text
arduino-cli compile --fqbn MiniCore:avr:8:clock=16MHz_external,BOD=2v7,eeprom=keep,LTO=Os_flto,bootloader=uart0,baudrate=default --libraries /home/aidancrowther/Arduino/libraries --export-binaries firmware/BirdSkeletonFixture
```

## Servo configuration

Each SG90 must have independent minimum and maximum positions. These values must be exposed near the top of the firmware in one clearly labeled configuration section so they can be tuned without searching through motion logic.

Servo calibration values are saved as angles in degrees and converted to pulse widths in microseconds at compile time. This keeps the mechanical settings easy to understand while allowing the runtime servo scheduler to use integer microsecond values.

Recommended firmware configuration:

```c
/* Easily located per-servo motion limits.
 * Values are mechanical angles and are converted to microseconds below.
 */
#define SERVO1_MIN_ANGLE  /* tune for mechanism */
#define SERVO1_MAX_ANGLE  /* tune for mechanism */
#define SERVO2_MIN_ANGLE  /* tune for mechanism */
#define SERVO2_MAX_ANGLE  /* tune for mechanism */
#define SERVO3_MIN_ANGLE  /* tune for mechanism */
#define SERVO3_MAX_ANGLE  /* tune for mechanism */
```

The actual angle values depend on the mechanical installation and must be calibrated to prevent the SG90s from driving against their end stops. The firmware clamps every requested position to the corresponding servo range before generating a pulse. Servos default to their minimum defined position at startup and on DMX timeout. Interpolation/slew limiting is used when moving between positions.

The servo signals are:

| Servo | Signal pin |
|---|---|
| Servo 1 | `PD2` |
| Servo 2 | `PD3` |
| Servo 3 | `PD4` |

SG90 power should use a suitable regulated supply with adequate current capacity and a common ground shared with the ATMega8. Do not power the servos directly from an ATMega8 I/O pin.

## WS2812 LED ring

- LED count: `7`.
- Data output: `PD5`.
- `PD5` connects to the `DIN` input of the first WS2812.
- The data chain proceeds from each LED’s `DOUT` to the next LED’s `DIN`.
- The MCU, LED supply, and LED data signal must share a common ground.
- LED power must be supplied separately from the MCU I/O pin and sized for the maximum LED current.

The firmware should treat the ring as a seven-pixel buffer and transmit exactly seven WS2812 pixel values per refresh.

## DFPlayer Mini

- MCU signal: `PD6`.
- DFPlayer connection: `PD6` connects to the DFPlayer Mini `RX` pin.
- DFPlayer `TX`: intentionally left disconnected.
- Communication direction: transmit-only from the ATMega8 to the DFPlayer Mini.
- Serial implementation: the installed `TXOnlySerial` library, configured for transmit-only software serial on `PD6`; the DFPlayer connection does not use a hardware UART transmit pin.

The firmware must use `TXOnlySerial` for the DFPlayer interface and must not depend on response packets or status feedback because the DFPlayer `TX` pin is not connected. Audio commands should be sent through the `TXOnlySerial` transmit interface on `PD6` using the DFPlayer serial protocol. `PD6` should not be reused by another peripheral.

Because `TXOnlySerial` is software serial, its bit timing must be coordinated with the DMX receiver, servo timing, and WS2812 transmission. The implementation should follow the installed library’s timing requirements and avoid interrupt or timing conflicts during DFPlayer byte transmission. The selected ATMega8 clock frequency and baud rate must be supported by the library configuration.

The DFPlayer power supply, speaker wiring, serial-level compatibility, and local decoupling should still be finalized in the electrical design.

## Firmware initialization requirements

At startup, firmware should:

1. Configure `PB1–PB5` and `PC0–PC2` as inputs.
2. Enable the ATMega8 internal pull-up on each DIP input.
3. Read and invert the active-low switch states.
4. Decode the DMX start address using fixed bit 0 = `1`.
5. Configure `PD2–PD4` as servo outputs.
6. Configure `PD5` as the WS2812 data output.
7. Configure `PD7` as the receive-only dummy mode pin required by the installed `DMXSerial` API.
8. Initialize `DMXSerial` in `DMXReceiver` mode using the installed ATmega8-compatible library and limit reception to the fixture footprint.
9. Initialize the installed `TXOnlySerial` library for transmit-only software serial on `PD6`, connected to the DFPlayer Mini `RX` input; do not expect response data because the DFPlayer `TX` pin is disconnected.
10. Initialize servos at their minimum defined positions.
11. Wait for a valid DMX frame before applying motion, lighting, or audio commands.

## Remaining implementation to-do items

The hardware and build configuration is now defined. The following project items remain intentionally configurable or incomplete:

- Numeric min/max values for each servo after mechanical calibration.
- Final DFPlayer SD-card track assignments. No specific track numbers are currently configured; track aliases remain a firmware to-do.
- Power supplies, current budgets, decoupling, and common-ground arrangement for the servos and WS2812 LEDs.