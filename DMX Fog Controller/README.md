# DMX Fog Controller

An ATmega8-based DMX receiver fixture for controlling up to four relay-driven
fog machines. The board receives DMX512 through a galvanically isolated MAX485
interface and provides four active-low relay outputs.

> **Safety:** This controller is intended to operate fog-machine control
> inputs through correctly rated relay modules. Fog machines may contain mains
> voltage, heaters, pumps, and hot surfaces. Do not connect or service the
> mains side while powered. Use appropriate enclosures, fusing, earthing,
> creepage/clearance, relay contact ratings, and local electrical practices.

## Hardware

- Microcontroller: ATmega8 / ATmega8A
- Clock: 16 MHz external oscillator/crystal configuration
- DMX input: isolated MAX485, permanently configured for receive
- DMX RX: `PD0/RXD`
- DMXSerial dummy direction output: `PD2` (`D2`), not connected to the MAX485
- Reserved diagnostic pin: `PD1/TXD` (`D1`)
- Relay outputs: active low; drive the MCU pin low to energize a relay

### Pin assignment

| ATmega8 pin | Arduino/MiniCore pin | Function |
|---|---:|---|
| `PD0/RXD` | `D0` | DMX receive data |
| `PD1/TXD` | `D1` | Reserved for future diagnostics |
| `PD2` | `D2` | DMXSerial dummy direction output; unused electrically |
| `PD5` | `D5` | Fog machine 2 relay |
| `PD6` | `D6` | Fog machine 3 relay |
| `PD7` | `D7` | Fog machine 4 relay |
| `PB0` | `D8` | Fog machine 1 relay |
| `PB1` | `D9` | Address bit 9 |
| `PB2` | `D10` | Address bit 8 |
| `PB3` | `D11` | Address bit 7 |
| `PB4` | `D12` | Address bit 6 |
| `PB5` | `D13` | Address bit 5 |
| `PC0` | `A0/D14` | Address bit 4 |
| `PC1` | `A1/D15` | Address bit 3 |
| `PC2` | `A2/D16` | Address bit 2 |
| `PB6/PB7` | — | 16 MHz external clock |

The eight address inputs are initialized with the ATmega8 internal pull-ups.
Grounding an input sets its bit. Address bit 1 is permanently set to one, so
only odd addresses can be selected.

The address is sampled once during startup. Reset or power-cycle the fixture
after changing the address switches.

## DMX personality

The fixture uses seven consecutive DMX channels beginning at the configured
start address.

| Offset | Function | Description |
|---:|---|---|
| 1 | Fog machine 1 command | Controls `PB0` |
| 2 | Fog machine 2 command | Controls `PD5` |
| 3 | Fog machine 3 command | Controls `PD6` |
| 4 | Fog machine 4 command | Controls `PD7` |
| 5 | Burst/cycle-on duration | Shared on-time, 0.25–30 seconds |
| 6 | Cycle-off duration | Shared off-time, 2–300 seconds |
| 7 | Master enable | `0–127` disabled, `128–255` enabled |

### Fog-machine command channels

| DMX value | Mode |
|---:|---|
| `0–9` | Off; cancel activity and arm automatic modes |
| `10–99` | Idle/deadband |
| `100–149` | One timed burst |
| `150–199` | Repeating cycle |
| `200–255` | Continuous fog |

Values `0–9` turn the relay off, cancel its active timer, and arm it for a
future burst or cycle. Values `10–99` are an idle deadband. Entering
`100–149` starts one burst if the command has visited `0–9`; the command is
then disarmed so holding it high does not repeatedly fire the machine. Entering
`150–199` starts a cycle under the same arm requirement. A cycle is:

```text
cycle-on duration -> cycle-off duration -> repeat
```

Values `200–255` provide continuous fog while the master is enabled and valid
DMX is present. Each machine has an independent timer and cycle phase.

Channel 5 maps linearly from 0.25 to 30 seconds:

```text
on-time-ms = 250 + channel5 * 29750 / 255
```

Channel 6 maps linearly from 2 to 300 seconds:

```text
off-time-ms = 2000 + channel6 * 298000 / 255
```

Channel 7 is a hard enable: `0–127` turns all relays off and cancels timers;
`128–255` permits normal operation. Enabling the master does not fire an
automatic command already held high; the command must first return to `0–9`.

## DMX addressing

The selected address is:

```text
1
+ (PB1 grounded ? 256 : 0)
+ (PB2 grounded ? 128 : 0)
+ (PB3 grounded ?  64 : 0)
+ (PB4 grounded ?  32 : 0)
+ (PB5 grounded ?  16 : 0)
+ (PC0 grounded ?   8 : 0)
+ (PC1 grounded ?   4 : 0)
+ (PC2 grounded ?   2 : 0)
```

Valid start addresses for this seven-channel fixture are odd values from
**1 through 505**. Switch combinations producing 507, 509, or 511 are
rejected and hold every relay off.

## Failsafe behavior

All relay outputs are initialized inactive before DMX reception begins. Since
the relay inputs are active low, inactive means the MCU pins are driven high.
If no valid
DMX packet has been received for one second, all relays turn off immediately,
all timers are cancelled, and automatic commands are disarmed. After DMX
returns, continuous commands may resume when the master is enabled; automatic
commands must return to `0–9` before they can trigger again.

Normal relay transitions are limited to one change per 100 ms to reduce
mechanical relay chatter. Safety shutdowns bypass this interval.

## Software and build

The sketch is located at:

```text
Code/DMX_Fog_Controller/DMX_Fog_Controller.ino
```

Required software:

- Arduino CLI
- MiniCore AVR core 3.0.2 or compatible
- `DMXSerial` 1.5.3 or compatible

DMXSerial uses the ATmega8 hardware USART interrupt for DMX reception. The
USART is therefore not available for ordinary `Serial.print()` debugging while
the fixture is running. `PD1/TXD` is intentionally left available for a future
software-UART or other diagnostic implementation.

From the project root, build for the installed MiniCore ATmega8 definition,
16 MHz external clock, no bootloader, and LTO with:

```sh
arduino-cli compile \
  --fqbn 'MiniCore:avr:8:clock=16MHz_external,bootloader=no_bootloader,LTO=Os_flto' \
  --libraries "$HOME/Arduino/libraries" \
  './Code/DMX_Fog_Controller'
```

The fuses must match the fitted clock hardware. Program the resulting image
through the board's ISP or bootloader workflow.

## Testing checklist

Before connecting fog machines, verify relay outputs at reset, test every
address bit, test master disable, continuous mode, one-shot and cycle modes,
and disconnect DMX to confirm the one-second failsafe. Also verify that
addresses 507, 509, and 511 safely inhibit the fixture.
