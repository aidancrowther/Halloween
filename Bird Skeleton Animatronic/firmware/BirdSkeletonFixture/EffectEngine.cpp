#include "EffectEngine.h"

#include "Config.h"

namespace {

enum Preset : uint8_t {
  PRESET_NONE,
  PRESET_FLAP,
  PRESET_ALTERNATING,
  PRESET_BEAK,
  PRESET_STARTLE,
  PRESET_TWITCH,
  PRESET_LIGHTNING,
  PRESET_BIRD,
  PRESET_DEMO
};

uint8_t selectedBand = 0xFF;
Preset activePreset = PRESET_NONE;
Preset lastRandomPreset = PRESET_NONE;
bool active = false;
bool repeating = false;
bool randomRepeating = false;
bool coolingDown = false;
bool soundIssued = false;
bool actionStarted = false;
uint8_t actionTrack = 1;
uint8_t actionLeft = 255;
uint8_t actionRight = 255;
uint8_t actionBeak = 255;
uint32_t actionStart = 0;
uint32_t cooldownStart = 0;
uint8_t randomState = 0xA7;
uint8_t twitchLeft = 0;
uint8_t twitchRight = 0;
uint8_t twitchBeak = 0;
uint8_t demoSegment = 0xFF;

uint8_t nextRandom()
{
  uint8_t bit = static_cast<uint8_t>(((randomState >> 7) ^ (randomState >> 5) ^
                                      (randomState >> 4) ^ (randomState >> 3)) & 1U);
  randomState = static_cast<uint8_t>((randomState << 1) | bit);
  return randomState;
}

uint16_t mapRange(uint8_t value, uint16_t minimum, uint16_t maximum)
{
  return minimum + (static_cast<uint32_t>(maximum - minimum) * value) / 255UL;
}

uint16_t mapBeakInput(uint8_t value)
{
  if (value <= BEAK_CLOSED_INPUT) return BEAK_MIN_US;
  if (value >= BEAK_OPEN_INPUT) return BEAK_MAX_US;
  return BEAK_MIN_US +
         (static_cast<uint32_t>(BEAK_MAX_US - BEAK_MIN_US) *
          (value - BEAK_CLOSED_INPUT)) /
         (BEAK_OPEN_INPUT - BEAK_CLOSED_INPUT);
}

uint8_t scale(uint8_t value, uint8_t amount)
{
  return static_cast<uint8_t>((static_cast<uint16_t>(value) * amount) / 255U);
}

uint8_t bandForMode(uint8_t mode)
{
  if (mode < 16) return 0;
  if (mode >= 240 && mode <= 247) return 15;
  if (mode >= 248 && mode <= 251) return 16;
  if (mode >= 252) return 17;
  return static_cast<uint8_t>(1U + ((mode - 16U) / 16U));
}

Preset presetForBand(uint8_t band, bool &isRepeating)
{
  isRepeating = false;
  if (band == 0) return PRESET_NONE;
  if (band >= 1 && band <= 7) return static_cast<Preset>(band);
  if (band >= 8 && band <= 14) {
    isRepeating = true;
    return static_cast<Preset>(band - 7U);
  }
  if (band == 15) return PRESET_DEMO;
  if (band == 16) { isRepeating = true; return PRESET_DEMO; }
  if (band == 17) { isRepeating = true; return PRESET_NONE; }
  return PRESET_NONE;
}

Preset randomPreset()
{
  Preset chosen;
  do {
    chosen = static_cast<Preset>(PRESET_FLAP + (nextRandom() % 7U));
  } while (chosen == lastRandomPreset);
  lastRandomPreset = chosen;
  return chosen;
}

uint8_t trackForPreset(Preset preset)
{
  switch (preset) {
    // Wing actions use the three flap/flutter files.
    case PRESET_FLAP:
    case PRESET_ALTERNATING: return static_cast<uint8_t>(5U + (nextRandom() % 3U));
    // Beak and bird actions use the four call/screech files.
    case PRESET_BEAK:
    case PRESET_BIRD: return static_cast<uint8_t>(1U + (nextRandom() % 4U));
    // Startle/twitch actions use the rattle/screech family.
    case PRESET_STARTLE:
    case PRESET_TWITCH: return static_cast<uint8_t>(8U + (nextRandom() & 1U));
    // Lightning keeps its deep-call family.
    case PRESET_LIGHTNING: return TRACK_LIGHTNING;
    case PRESET_DEMO: return TRACK_WING_FLAP;
    default: return TRACK_RANDOM_BIRD;
  }
}

uint32_t durationFor(Preset preset)
{
  switch (preset) {
    case PRESET_FLAP: return 1400UL;
    case PRESET_ALTERNATING: return 1800UL;
    case PRESET_BEAK: return 1200UL;
    case PRESET_STARTLE: return 1200UL;
    case PRESET_TWITCH: return 1800UL;
    case PRESET_LIGHTNING: return 1100UL;
    case PRESET_BIRD: return 1800UL;
    case PRESET_DEMO: return 8000UL;
    default: return 0;
  }
}

uint8_t envelope(uint32_t elapsed, uint32_t attack, uint32_t hold, uint32_t release,
                 uint32_t &total)
{
  total = attack + hold + release;
  if (elapsed >= total) return 0;
  if (elapsed < attack) return static_cast<uint8_t>((elapsed * 255UL) / attack);
  if (elapsed < attack + hold) return 255;
  return static_cast<uint8_t>(255UL -
                              ((elapsed - attack - hold) * 255UL) / release);
}

void setPresetLight(LedEngine::Frame &leds, uint8_t red, uint8_t green,
                    uint8_t blue, uint8_t dimmer)
{
  leds.red = red;
  leds.green = green;
  leds.blue = blue;
  leds.dimmer = dimmer;
  leds.effect = 0;
  leds.rate = 0;
}

void beginAction(Preset preset, bool shouldRepeat, uint32_t now,
                 const uint8_t *channels)
{
  activePreset = preset == PRESET_NONE ? randomPreset() : preset;
  repeating = shouldRepeat;
  active = activePreset != PRESET_NONE;
  coolingDown = false;
  actionStart = now;
  soundIssued = false;
  actionStarted = false;
  actionTrack = trackForPreset(activePreset);
  demoSegment = 0xFF;
  actionLeft = channels[CHANNEL_LEFT_WING];
  actionRight = channels[CHANNEL_RIGHT_WING];
  actionBeak = channels[CHANNEL_BEAK];
  if (activePreset == PRESET_TWITCH) {
    twitchLeft = nextRandom();
    twitchRight = nextRandom();
    twitchBeak = nextRandom();
  }
}

void idleOutput(const uint8_t *channels, EffectEngine::Output &output)
{
  output.servos = {LEFT_WING_PARK_US, RIGHT_WING_PARK_US, BEAK_PARK_US};
  output.leds = {0, 0, 0, 0, 0, 0};
  output.soundTrack = 0;
  output.servoStepUs = static_cast<uint16_t>(SERVO_SLEW_MIN_US +
      (static_cast<uint32_t>(SERVO_SLEW_MAX_US - SERVO_SLEW_MIN_US) *
       channels[CHANNEL_SPEED]) / 255UL);
}

void actionOutput(uint32_t now, EffectEngine::Output &output)
{
  output.servos = {LEFT_WING_PARK_US, RIGHT_WING_PARK_US, BEAK_PARK_US};
  output.leds = {0, 0, 0, 0, 0, 0};
  output.soundTrack = soundIssued ? 0 : actionTrack;
  output.servoStepUs = SERVO_SLEW_MAX_US;
  if (!actionStarted) return;

  const uint32_t elapsed = now >= actionStart ? now - actionStart : 0;
  const uint32_t duration = durationFor(activePreset);

  const uint8_t leftAmount = actionLeft;
  const uint8_t rightAmount = actionRight;
  const uint8_t beakAmount = actionBeak;
  uint8_t motion = 0;
  uint8_t secondary = 0;
  uint32_t total = 0;

  switch (activePreset) {
    case PRESET_FLAP:
      motion = envelope(elapsed, 350, 200, 850, total);
      output.servos.left = mapRange(scale(motion, leftAmount), LEFT_WING_MIN_US, LEFT_WING_MAX_US);
      output.servos.right = mapRange(scale(motion, rightAmount), RIGHT_WING_MIN_US, RIGHT_WING_MAX_US);
      output.servos.beak = mapRange(scale(motion, beakAmount / 2U), BEAK_MIN_US, BEAK_MAX_US);
      setPresetLight(output.leds, 160, 210, 255, motion);
      break;
    case PRESET_ALTERNATING:
      if (elapsed < 900) {
        motion = envelope(elapsed, 300, 150, 450, total);
        output.servos.left = mapRange(scale(motion, leftAmount), LEFT_WING_MIN_US, LEFT_WING_MAX_US);
      } else {
        motion = envelope(elapsed - 900UL, 300, 150, 450, total);
        output.servos.right = mapRange(scale(motion, rightAmount), RIGHT_WING_MIN_US, RIGHT_WING_MAX_US);
      }
      setPresetLight(output.leds, 80, 170, 255, motion);
      break;
    case PRESET_BEAK:
      motion = envelope(elapsed, 250, 250, 700, total);
      output.servos.left = mapRange(scale(motion, leftAmount / 3U), LEFT_WING_MIN_US, LEFT_WING_MAX_US);
      output.servos.right = mapRange(scale(motion, rightAmount / 3U), RIGHT_WING_MIN_US, RIGHT_WING_MAX_US);
      output.servos.beak = mapRange(scale(motion, beakAmount), BEAK_MIN_US, BEAK_MAX_US);
      setPresetLight(output.leds, 255, 120, 20, motion);
      break;
    case PRESET_STARTLE:
      motion = envelope(elapsed, 180, 180, 840, total);
      output.servos.left = mapRange(scale(motion, leftAmount), LEFT_WING_MIN_US, LEFT_WING_MAX_US);
      output.servos.right = mapRange(scale(motion, rightAmount), RIGHT_WING_MIN_US, RIGHT_WING_MAX_US);
      output.servos.beak = mapRange(scale(motion, beakAmount), BEAK_MIN_US, BEAK_MAX_US);
      setPresetLight(output.leds, 255, elapsed < 220 ? 255 : 20,
                     elapsed < 220 ? 255 : 20, elapsed < 220 ? 255 : motion);
      output.servoStepUs = SERVO_SLEW_MAX_US;
      break;
    case PRESET_TWITCH:
      if (elapsed < 400) {
        motion = static_cast<uint8_t>((elapsed * 255UL) / 400UL);
      } else if (elapsed < 700) {
        motion = 255;
      } else if (elapsed < 1200) {
        motion = static_cast<uint8_t>(255UL - ((elapsed - 700UL) * 95UL) / 500UL);
      } else if (elapsed < 1600) {
        motion = static_cast<uint8_t>(160UL - ((elapsed - 1200UL) * 160UL) / 400UL);
      }
      output.servos.left = mapRange(scale(motion, twitchLeft), LEFT_WING_MIN_US, LEFT_WING_MAX_US);
      output.servos.right = mapRange(scale(motion, twitchRight), RIGHT_WING_MIN_US, RIGHT_WING_MAX_US);
      output.servos.beak = mapRange(scale(motion, twitchBeak), BEAK_MIN_US, BEAK_MAX_US);
      setPresetLight(output.leds, 110, 40, 190, motion);
      break;
    case PRESET_LIGHTNING:
      motion = elapsed < 120 ? 255 : (elapsed < 260 ? 0 : (elapsed < 360 ? 150 : 0));
      output.servos.left = mapRange(scale(motion, leftAmount), LEFT_WING_MIN_US, LEFT_WING_MAX_US);
      output.servos.right = mapRange(scale(motion, rightAmount), RIGHT_WING_MIN_US, RIGHT_WING_MAX_US);
      output.servos.beak = mapRange(scale(motion, beakAmount), BEAK_MIN_US, BEAK_MAX_US);
      setPresetLight(output.leds, motion, motion, motion, motion);
      output.servoStepUs = SERVO_SLEW_MAX_US;
      break;
    case PRESET_BIRD:
      motion = envelope(elapsed, 350, 500, 950, total);
      secondary = static_cast<uint8_t>((elapsed / 220U) & 1U ? motion : 0);
      output.servos.left = mapRange(scale(motion, leftAmount / 2U), LEFT_WING_MIN_US, LEFT_WING_MAX_US);
      output.servos.right = mapRange(scale(motion, rightAmount / 2U), RIGHT_WING_MIN_US, RIGHT_WING_MAX_US);
      output.servos.beak = mapRange(scale(secondary, beakAmount), BEAK_MIN_US, BEAK_MAX_US);
      setPresetLight(output.leds, 255, 130, 20, motion);
      break;
    case PRESET_DEMO: {
      const uint8_t segment = static_cast<uint8_t>(elapsed / 2000UL);
      const uint32_t phase = elapsed % 2000UL;
      if (segment != demoSegment) {
        demoSegment = segment;
      }
      motion = envelope(phase, 300, 250, 1450, total);
      if (segment == 0) {
        output.servos.left = mapRange(scale(motion, leftAmount), LEFT_WING_MIN_US, LEFT_WING_MAX_US);
        output.servos.right = mapRange(scale(motion, rightAmount), RIGHT_WING_MIN_US, RIGHT_WING_MAX_US);
        output.leds = {160, 210, 255, motion, 0, 0};
      } else if (segment == 1) {
        output.servos.left = mapRange(scale(motion, leftAmount), LEFT_WING_MIN_US, LEFT_WING_MAX_US);
        output.leds = {80, 170, 255, motion, 0, 0};
      } else if (segment == 2) {
        output.servos.beak = mapRange(scale(motion, beakAmount), BEAK_MIN_US, BEAK_MAX_US);
        output.leds = {255, 120, 20, motion, 0, 0};
      } else {
        const uint8_t flash = phase < 180 ? 255 : 0;
        output.servos.left = mapRange(scale(flash, leftAmount), LEFT_WING_MIN_US, LEFT_WING_MAX_US);
        output.servos.right = mapRange(scale(flash, rightAmount), RIGHT_WING_MIN_US, RIGHT_WING_MAX_US);
        output.leds = {flash, flash, flash, flash, 0, 0};
        output.servoStepUs = SERVO_SLEW_MAX_US;
      }
      break;
    }
    default:
      break;
  }

  if (elapsed >= duration) {
    active = false;
    coolingDown = repeating;
    cooldownStart = now;
  }
}

}

namespace EffectEngine {

void begin()
{
  activePreset = PRESET_NONE;
  lastRandomPreset = PRESET_NONE;
  active = false;
  repeating = false;
  randomRepeating = false;
  coolingDown = false;
}

void cancel(uint32_t now)
{
  activePreset = PRESET_NONE;
  active = false;
  repeating = false;
  randomRepeating = false;
  coolingDown = false;
  soundIssued = false;
  actionStarted = false;
  actionStart = now;
  cooldownStart = now;
}

void update(const uint8_t *channels, uint32_t now, Output &output)
{
  const uint8_t mode = channels[CHANNEL_MODE];
  const uint8_t band = bandForMode(mode);

  // While an action is running, the live DMX stream is only monitored. The
  // action owns its latched command until the finite timeline completes.
  if (!active && band != selectedBand) {
    selectedBand = band;
    bool nextRepeating = false;
    Preset nextPreset = presetForBand(band, nextRepeating);
    active = false;
    coolingDown = false;
    randomRepeating = band == 17;
    if (nextPreset == PRESET_NONE && band == 17) {
      beginAction(PRESET_NONE, true, now, channels);
    } else if (nextPreset != PRESET_NONE) {
      beginAction(nextPreset, nextRepeating, now, channels);
    }
  }

  if (band == 0) {
    output.servos.left = mapRange(channels[CHANNEL_LEFT_WING], LEFT_WING_MIN_US, LEFT_WING_MAX_US);
    output.servos.right = mapRange(channels[CHANNEL_RIGHT_WING], RIGHT_WING_MIN_US, RIGHT_WING_MAX_US);
    output.servos.beak = mapBeakInput(channels[CHANNEL_BEAK]);
    output.leds = {channels[CHANNEL_RED], channels[CHANNEL_GREEN], channels[CHANNEL_BLUE],
                   channels[CHANNEL_DIMMER], channels[CHANNEL_LED_EFFECT], channels[CHANNEL_EFFECT_RATE]};
    output.soundTrack = 0;
    output.servoStepUs = static_cast<uint16_t>(SERVO_SLEW_MIN_US +
        (static_cast<uint32_t>(SERVO_SLEW_MAX_US - SERVO_SLEW_MIN_US) *
         channels[CHANNEL_SPEED]) / 255UL);
    return;
  }

  if (!active && coolingDown && repeating && now - cooldownStart >= PRESET_COOLDOWN_MS) {
    beginAction(randomRepeating ? PRESET_NONE : activePreset, true, now, channels);
  }

  if (active) {
    actionOutput(now, output);
  } else {
    idleOutput(channels, output);
  }
}

void audioRequested()
{
  soundIssued = true;
}

void audioTransmitted(uint32_t now)
{
  actionStart = now;
  actionStarted = true;
}

bool running()
{
  return active;
}

bool modeLocked()
{
  return active || coolingDown;
}

}
