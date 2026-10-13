# TODO / Future Additions

This document collects future improvements and content additions for the bird-skeleton DMX fixture. It is intentionally separate from the production firmware documentation: the items below are proposals and are not implemented unless explicitly marked as complete.

## Current status

### Audio content is not configured

The firmware currently contains these DFPlayer aliases:

```cpp
TRACK_RANDOM_BIRD
TRACK_LIGHTNING
TRACK_WING_FLAP
TRACK_CAGE_RATTLE
TRACK_CAW
TRACK_SCREECH
```

All six are currently set to `0` in `Config.h`:

```cpp
constexpr uint16_t TRACK_RANDOM_BIRD = 0;
constexpr uint16_t TRACK_LIGHTNING = 0;
constexpr uint16_t TRACK_WING_FLAP = 0;
constexpr uint16_t TRACK_CAGE_RATTLE = 0;
constexpr uint16_t TRACK_CAW = 0;
constexpr uint16_t TRACK_SCREECH = 0;
```

`AudioEngine::requestTrack()` ignores track number `0`, so the named mode-triggered sounds are currently suppressed. Direct sound-channel values from `1` through `200` can still request a track, but those track numbers must exist on the DFPlayer SD card.

The command values already reserved by the firmware are:

| DMX sound value | Action |
|---:|---|
| 1–200 | Play the selected track number |
| 240 | DFPlayer command `0x16` |
| 241 | DFPlayer command `0x0E` |
| 242 | DFPlayer command `0x0D` |
| 245 | Random-bird alias |
| 246 | Lightning alias |
| 247 | Wing-flap alias |
| 248 | Cage-rattle alias |
| 249 | Caw alias |
| 250 | Screech alias |

## Recommended first sound pack

The first pack should use short, mono, normalized clips rather than long ambience. The DFPlayer is being used as an event player, and short clips reduce the temptation to trigger audio repeatedly while also reducing the time spent transmitting commands during a DMX universe.

| Alias | Suggested content | Desired duration | Suggested source/search terms |
|---|---|---:|---|
| `TRACK_RANDOM_BIRD` | 3–6 varied short bird calls or unsettling chirps | 0.5–2.0 s each | crow, raven, rook, distressed bird, isolated chirp |
| `TRACK_LIGHTNING` | Thunder crack, electrical snap, or short storm hit | 0.5–3.0 s | thunder crack, lightning, electrical arc, storm hit |
| `TRACK_WING_FLAP` | Dry feather flap, cloth flap, or two-beat wing movement | 0.2–1.0 s | wing flap, feather movement, bird flap |
| `TRACK_CAGE_RATTLE` | Metal-bar rattle, chain shake, or short cage impact | 0.3–1.5 s | cage rattle, metal bars, chain rattle, wire fence |
| `TRACK_CAW` | Clear crow/raven caw with a few alternate takes | 0.5–2.0 s | crow caw, raven call, rook call |
| `TRACK_SCREECH` | Harsh bird screech or startled cry | 0.5–2.0 s | bird screech, raven alarm, distressed bird |

### Recommended licensing strategy

Prefer one of these options, in order:

1. **Self-recorded sounds**. Keep the original recordings and a note describing where and when they were captured.
2. **CC0 / public-domain recordings**. These are simplest for a project that may eventually be shown, hired out, or sold.
3. **CC BY recordings**. Acceptable if the project keeps a clear attribution file.
4. Avoid **CC BY-NC** unless the fixture will definitely remain non-commercial.

Freesound hosts sounds under multiple Creative Commons licenses; the license must be checked for each individual download rather than assumed from the website as a whole. Its [official FAQ](https://freesound.org/help/faq/) identifies CC0, CC BY, and CC BY-NC as the main license categories and explains the attribution and non-commercial implications.

Potential starting points:

- **[Crows Singing](https://opengameart.org/content/crows-singing)** on OpenGameArt: listed as CC0 and directly relevant to `TRACK_CAW` and possibly `TRACK_RANDOM_BIRD`.
- **[Vulture Imitation](https://opengameart.org/content/vulture-imitation)** on OpenGameArt: listed as CC0 and useful as an eerie creature-call layer or screech source.
- **[Solo Seagull Sound Effects](https://opengameart.org/node/92772)** on OpenGameArt: listed as CC0 and useful for testing call variation even if the final bird is not a seagull.
- **[Animal Sounds](https://opengameart.org/content/animal-sounds)** on OpenGameArt: a CC0-oriented collection page that may provide additional bird/creature prototypes.
- Freesound searches filtered to **CC0** or **CC BY** for thunder, cage/metal rattle, feather movement, and bird-call variations.

The OpenGameArt pages above identify the listed assets as CC0 at the time of review. Retain a copy of the download page, asset name, author, license, and download date in the project credits before using any file. Licenses and source pages can change, so verify them again when assembling the final SD card.

### SD-card content and credits

Create an audio inventory before copying files to the DFPlayer card:

```text
audio/
  0001_caw_primary.mp3
  0002_caw_low.mp3
  0003_caw_short.mp3
  0010_screech_primary.mp3
  0020_wing_flap.mp3
  0030_cage_rattle.mp3
  0040_lightning_crack.mp3
  0050_thunder_short.mp3
  0060_bird_random_01.mp3
  0061_bird_random_02.mp3
  0062_bird_random_03.mp3
  CREDITS.md
```

The exact directory and filename convention must be confirmed against the installed DFPlayer library/module behavior before finalizing the card. Keep the numeric track ID unambiguous, and avoid relying on alphabetical filename ordering.

`CREDITS.md` should record:

- Track number and final filename.
- Original asset title and creator.
- Source page.
- License.
- Required attribution text.
- Any edits performed: trimming, normalization, layering, pitch shift, or noise removal.

## Audio firmware improvements

### High priority

- Assign real track numbers only after the SD-card pack is tested.
- Add a startup/self-test mode that plays one known test track only when explicitly requested.
- Add a configurable audio enable/disable option for quiet rehearsals.
- Add per-event cooldowns so repeated DMX changes cannot retrigger the same sound rapidly.
- Add a separate volume-change deadband so tiny DMX volume fluctuations do not create repeated DFPlayer commands.
- Confirm whether command `0x16`, `0x0E`, and `0x0D` are intended as pause, stop, and playback controls for the chosen DFPlayer firmware.
- Document the exact track-number and SD-card format expected by the installed DFPlayer module.

### Variation and realism

- Map one alias to a small bank of alternate tracks instead of replaying the same clip.
- Choose the alternate clip using the existing pseudo-random generator.
- Add a minimum interval between random-bird, caw, and screech events.
- Add optional “call and response” behavior: a short call followed by a delayed answer.
- Add a low-volume ambient loop only if it can be started/stopped without undermining DMX timing.
- Add audio-specific DMX controls for attack intensity, variation amount, and cooldown.

### Timing protection

- Record the DMX packet-complete timestamp and defer DFPlayer transmission until a deliberate safe window.
- Prefer sending audio commands immediately after the fixture’s relevant DMX channels have been copied, while avoiding the next break when possible.
- If exact DMX integrity is required, consider moving DFPlayer output to a hardware-UART-compatible arrangement or a serial implementation that does not mask interrupts for every transmitted bit.
- Test audio commands with every DMX slot changing continuously, not with mostly zero-valued universes.

## LED and DMX improvements

### High priority

- Add a phase-sweep test to the simavr profiler so WS2812 transfers are deliberately placed at different offsets within the DMX universe.
- Run a real-DMX logic-analyzer test with start addresses `1`, `173`, `255`, and `499`.
- Verify whether a WS2812 transfer that overlaps DMX causes only a stale snapshot or can produce a partially shifted channel map.
- Consider deferring LED refreshes until after the fixture’s relevant DMX channels have arrived.
- Consider suppressing a refresh when a DMX update is pending or when the fixture is in a critical audio transition.

### Effect additions

- Add a slow breathing effect with a configurable minimum brightness.
- Add a two-color warning/pulse effect for lightning or alarm states.
- Add a low-power “eyes closed” or dormant effect.
- Add a configurable LED gamma curve or brightness table if visual linearity matters.
- Add a deliberate random sparkle density control rather than using only the current effect-rate comparison.
- Add a frame counter or diagnostic mode to verify the intended refresh rate on hardware.

## Servo and animation improvements

- Complete mechanical calibration of all three servo endpoints.
- Add per-servo inversion and trim values if the installed linkage is asymmetric.
- Add acceleration/deceleration limiting instead of only a fixed positional slew step.
- Add a servo diagnostic mode that moves one axis at a time through safe limits.
- Add a startup park delay or soft-start sequence to avoid all three servos moving abruptly at power-up.
- Measure servo edge jitter during WS2812 and DFPlayer activity with a logic analyzer.
- Add a configurable neutral pose for timeout and invalid-DMX behavior.

## DMX and control features

- Add a configurable DMX timeout response: park, freeze last pose, dim LEDs, mute audio, or a combination.
- Add an optional DMX “blackout” behavior that suppresses LED and audio activity without changing the servo pose.
- Add a personality/version identifier for future channel-map changes.
- Add a test mode that flashes the active start address using the LED ring.
- Add optional address verification at startup using a short LED color code.
- Consider a second personality with fewer channels for simpler lighting controllers.
- Decide whether all DMX values should be latched only after a complete fixture snapshot is available.

## Hardware and reliability additions

- Add brown-out testing under servo and LED load.
- Confirm regulated supplies, decoupling, and common-ground paths for the MCU, servos, WS2812 ring, and DFPlayer.
- Add a logic-analyzer test point or temporary diagnostic header for DMX RX, WS2812 data, DFPlayer TX, and one servo output.
- Measure actual current draw with all LEDs bright and all servos moving.
- Add a fuse or current limiting appropriate to the final power design.
- Verify DFPlayer startup time and behavior with the SD card inserted before enabling audio triggers.
- Test behavior after unplug/replug, brownout, and a malformed DMX break.

## Tooling and test additions

- Add randomized full-universe DMX data to the simavr profiler.
- Track complete, rejected, and partially copied DMX snapshots separately.
- Add a phase matrix for LED refresh versus DMX break, relevant channel range, and servo compare events.
- Add a phase matrix for one and two DFPlayer commands per universe.
- Add SRAM stack high-water instrumentation for a hardware test build.
- Keep production and profiling builds clearly separated.
- Preserve profiler output alongside each firmware release so timing regressions can be detected.

## Suggested implementation order

1. Select and license the first six audio categories.
2. Create the SD-card inventory and credits file.
3. Assign track constants and test direct sound-channel playback.
4. Add audio cooldown/deadband behavior.
5. Add randomized full-universe DMX data to the simulator.
6. Add LED-refresh deferral or phase testing based on measured hardware behavior.
7. Calibrate servo limits and add trims.
8. Add hardware diagnostics and power/reliability tests.
9. Re-run the full simavr profile and logic-analyzer tests before changing the production defaults.

## No production changes made

This document is planning material only. Creating it does not assign tracks, alter DMX behavior, change LED effects, or modify the production firmware.
