# Bird Skeleton Component Test Firmware

This is a standalone hardware test sketch. It is deliberately located outside the production sketch directory:

```text
firmware/tests/ComponentTest/ComponentTest.ino
```

It does **not** include, initialize, or require DMX input.

The test uses the exact production `ServoEngine`, `LedEngine`, `AudioEngine`,
and `Config.h` implementations through local bridge files. Its local `DmxInput`
stub makes the production output drivers link without initializing DMX, keeping
this standalone component test independent of the fixture receiver.

## Pin assignments

| Component | Arduino pin | ATmega8 port |
|---|---:|---|
| Left servo | 2 | PD2 |
| Right servo | 3 | PD3 |
| Beak servo | 4 | PD4 |
| WS2812 data | 5 | PD5 |
| DFPlayer TX | 6 | PD6 |

The serial monitor uses the ATmega8 hardware UART at 9600 baud for status messages. Do not connect the DFPlayer to that UART; the DFPlayer uses the separate transmit-only pin 6.

## Test behavior

- The wing servos continuously sweep from `8°` to `172°` and back. The beak
  continuously sweeps from `113°` to `180°` and back using the production
  `ServoEngine`.
- The seven WS2812 LEDs show all red, then all green, then all blue, repeating once per second at approximately 25% brightness.
- The DFPlayer receives a request for command `0x03` with parameter `0`, exactly as requested. The command is repeated every 10 seconds.
- DFPlayer volume commands are queued through the production `AudioEngine` and ramp from `0` through `30`, then wrap back to `0`.
- Status messages are printed on the hardware serial port.

### Track/file 0 caveat

DFPlayer SD-card track numbering is commonly treated as one-based by libraries and file layouts. This test intentionally sends parameter `0` because the requested test is specifically for file/track `0`. If the module does not respond audibly, place a known test file in the expected DFPlayer layout and temporarily change the parameter to `1` for a hardware comparison.

## Compile

From the project root:

```text
arduino-cli compile \
  --fqbn MiniCore:avr:8:clock=16MHz_external,BOD=2v7,eeprom=keep,LTO=Os_flto,bootloader=uart0,baudrate=default \
  --libraries /home/aidancrowther/Arduino/libraries \
  --export-binaries \
  firmware/tests/ComponentTest
```

The production firmware under `firmware/BirdSkeletonFixture/` is not modified. Its implementation files are intentionally included by the test build through the bridge files in this directory.