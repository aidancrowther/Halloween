# ATmega8 Fixture Profiling

## Scope

This report profiles the production ATmega8 firmware at 16 MHz using `simavr`.
The production firmware was not instrumented or modified for these tests. The
simulator loads the generated HEX image and drives the DIP inputs and DMX UART
externally.

The profiler source is:

```text
/home/aidancrowther/Documents/Projects/Bird Skeleton Animatronic/tools/simavr_profile.c
```

Run the complete profile with:

```bash
/home/aidancrowther/Documents/Projects/Bird Skeleton Animatronic/tools/run_simavr_profile.sh
```

The script rebuilds the production image before running the simulator.

## Build verification

The tested production configuration is MiniCore ATmega8, external 16 MHz,
Arduino bootloader, and LTO:

```text
Flash: 5906 / 7808 bytes
SRAM:   695 / 1024 bytes
```

The HEX used for the final run had SHA-256:

```text
fa57bba75e3e3b758b785702a6c345285011d7271dafd170571c9d1a010e53bd
```

## Simulation model

The profiler uses:

- ATmega8 model at 16 MHz.
- DMX slot spacing of 44 microseconds, equivalent to 250 kbaud and an
  11-bit DMX character.
- 1.728 ms combined break/MAB model.
- Full 512-slot universes.
- Framing-error UART input for the DMX break.
- Active-low DIP switch values for start addresses 1, 173, 255, 499, 501,
  and 511.
- 0.5 seconds of warm-up followed by approximately 0.5 seconds of measured
  execution per case.
- Approximately 23 universes per measured case.

The reported cycle percentages are classified from the linked production ELF
address ranges. They are simulated instruction-cycle estimates, not physical
oscilloscope measurements.

## Steady-state results

The firmware is a busy-loop design, so ordinary idle CPU percentage is not a
useful metric. The report therefore uses foreground loop rate plus cycle share
for the major interrupt and blocking-output paths.

At start address 1, the measured loop rates were:

| Mode | Foreground iterations/s | DMX RX ISR | Timer0 ISR | Servo ISR | LED blocking share |
|---|---:|---:|---:|---:|---:|
| manual | 5,387 | 14.244% | 0.573% | 0.119% | 0.000% |
| idle | 4,011 | 14.244% | 0.573% | 0.119% | 0.713% |
| synchronized | 4,041 | 14.244% | 0.573% | 0.119% | 0.717% |
| alternating | 3,276 | 14.244% | 0.573% | 0.119% | 0.000%* |
| chatter | 3,223 | 14.244% | 0.573% | 0.119% | 0.707% |
| startled | 2,387 | 14.244% | 0.573% | 0.119% | 0.716% |
| haunted | 2,211 | 14.244% | 0.573% | 0.119% | 0.724% |
| lightning | 2,301 | 14.244% | 0.573% | 0.119% | 0.709% |
| bird-call | 2,255 | 14.244% | 0.573% | 0.119% | 0.718% |
| demo | 3,399 | 14.244% | 0.573% | 0.119% | 0.397% |

`alternating` produced no changed LED frame during the sampled phase, so the
unchanged-frame suppression avoided a transfer. It is not a guarantee for all
phases.

All valid-address steady-state cases consumed 23/23 complete snapshots.

## Start-address results

| Start | Last used slot | Approximate snapshot availability | DMX RX ISR share | Loop rate |
|---:|---:|---:|---:|---:|
| 1 | 13 | 724 microseconds | 14.244% | 5,387/s |
| 173 | 185 | 8.292 ms | 15.097% | 5,334/s |
| 255 | 267 | 11.900 ms | 15.504% | 5,307/s |
| 499 | 511 | 22.636 ms | 16.715% | 5,230/s |

The receive ISR still runs for the rest of the universe after the fixture's
last channel. Therefore the cost has a large fixed per-universe component plus
an address-dependent component. It is not simply proportional to the number of
channels copied by the foreground loop.

The highest valid odd start address for the 13-channel footprint is 499.
Addresses 501 and 511 correctly enter invalid-address handling.

## WS2812 timing

The seven-pixel transfer measured approximately 288-299 microseconds with
interrupts disabled. The nominal wire time is approximately 216 microseconds;
the difference is compiled loop and call overhead.

At a continuously changing 25 Hz LED rate, the transfer occupied about
0.71-0.72% of the simulated processor time. Each transfer can cover roughly
six to seven 44-microsecond DMX slots.

## DFPlayer timing

The explicit `audio+led` case caused a track/volume transition. It measured:

- 3.600% cycle share in `TXOnlySerial`.
- 66 observed PD6 transitions.
- 939.6 microseconds maximum continuously interrupt-disabled segment.
- 0.724% LED-transfer blocking share.
- 2,094 foreground iterations/s.

The configured track aliases are currently zero, so ordinary mode cases do not
generate recurring track packets. Audio must therefore be profiled using an
explicit sound/volume transition, as done by the profiler.

## Cooperative versus fixed-rate DMX

The cooperative run lets the simulated source wait while simavr reports the
UART input FIFO as full. In the explicit audio case this produced 18 waits,
48.7 ms total, and an 8.28 ms maximum wait. These numbers demonstrate timing
contention but are not physical DMX delays; a real DMX controller does not
pause for receiver back-pressure.

The fixed-rate run ignores the simulated XON/XOFF state. It produced 2,161
simavr UART overrun diagnostics across the matrix. In the targeted audio case,
659 bytes were presented while the modeled receive FIFO was full. The sampled
test pattern still completed 23/23 snapshots because most dropped bytes were
redundant zero-valued slots and the next break resynchronized reception.

This result must not be interpreted as proof that arbitrary DMX data survives.
It demonstrates that the receiver can lose UART bytes during a DFPlayer or
WS2812 critical section. A real-world test should use changing data in every
slot and verify whether a damaged universe is rejected or partially consumed.

## Watchdog

No simulated watchdog reset occurred in the tested steady-state, invalid-address,
or audio-overlap cases. The longest measured single foreground interrupt-disabled
window was below 1 ms, while the watchdog period is 1 second.

The principal risks are therefore DMX byte loss, delayed snapshot completion,
and servo/DMX timing interaction—not watchdog starvation.

## Limitations and required hardware validation

Simavr provides cycle-accurate instruction execution for this test model, but
the following still require hardware validation:

1. Measure actual UART framing and overrun behavior with a logic analyzer.
2. Measure WS2812 timing and confirm the LED receiver accepts the waveform.
3. Measure servo edge jitter while DMX, LED, and DFPlayer activity overlap.
4. Use a real DMX source with nonzero changing values across the entire
   universe to quantify damaged-frame recovery.
5. Confirm the installed TXOnlySerial timing at the actual board clock.

No production firmware change was made as a result of this profiling pass.