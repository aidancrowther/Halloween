#include <avr/wdt.h>

#include "AudioEngine.h"
#include "Config.h"
#include "DmxInput.h"
#include "EffectEngine.h"
#include "LedEngine.h"
#include "ServoEngine.h"

namespace {

uint16_t currentLeft = LEFT_WING_PARK_US;
uint16_t currentRight = RIGHT_WING_PARK_US;
uint16_t currentBeak = BEAK_PARK_US;
bool timeoutActive = false;
uint8_t stableMode = 0;
uint8_t candidateMode = 0;
uint8_t candidateFrames = 0;

uint16_t moveToward(uint16_t current, uint16_t target, uint16_t step)
{
  if (current < target) {
    uint16_t difference = target - current;
    return current + (difference < step ? difference : step);
  }
  if (current > target) {
    uint16_t difference = current - target;
    return current - (difference < step ? difference : step);
  }
  return current;
}

void updateServos(const EffectEngine::ServoTargets &targets, uint16_t step)
{
  static uint32_t lastUpdate = 0;
  uint32_t now = millis();
  if (now - lastUpdate < SERVO_UPDATE_INTERVAL_MS) return;
  if (!ServoEngine::inQuietPeriod()) return;
  lastUpdate = now;

  currentLeft = moveToward(currentLeft, targets.left, step);
  currentRight = moveToward(currentRight, targets.right, step);
  currentBeak = moveToward(currentBeak, targets.beak, step);
  ServoEngine::setPulseWidths(currentLeft, currentRight, currentBeak);
}

void updateAudio(bool allowStart)
{
  static uint8_t previousSound = 0;
  uint8_t sound = DmxInput::value(CHANNEL_SOUND);

  if (sound != previousSound) {
    bool accepted = false;
    if (sound >= 1 && sound <= 200) accepted = AudioEngine::requestTrack(sound);
    else if (sound == 240) accepted = AudioEngine::requestCommand(0x16);
    else if (sound == 241) accepted = AudioEngine::requestCommand(0x0E);
    else if (sound == 242) accepted = AudioEngine::requestCommand(0x0D);
    else if (sound == 245) accepted = AudioEngine::requestTrack(TRACK_RANDOM_BIRD);
    else if (sound == 246) accepted = AudioEngine::requestTrack(TRACK_LIGHTNING);
    else if (sound == 247) accepted = AudioEngine::requestTrack(TRACK_WING_FLAP);
    else if (sound == 248) accepted = AudioEngine::requestTrack(TRACK_CAGE_RATTLE);
    else if (sound == 249) accepted = AudioEngine::requestTrack(TRACK_CAW);
    else if (sound == 250) accepted = AudioEngine::requestTrack(TRACK_SCREECH);
    else accepted = true;
    if (accepted) previousSound = sound;
  }

  AudioEngine::setVolume(static_cast<uint8_t>((static_cast<uint16_t>(DmxInput::value(CHANNEL_VOLUME)) * 30U) / 255U));
}

}

void setup()
{
  DmxInput::configureAddressPins();
  wdt_disable();
  DmxInput::begin();
  AudioEngine::begin();
  LedEngine::begin();
  ServoEngine::begin(LEFT_WING_PARK_US, RIGHT_WING_PARK_US, BEAK_PARK_US);
  EffectEngine::begin();
  wdt_enable(WDTO_1S);
}

void loop()
{
  wdt_reset();
  uint32_t now = millis();

  if (!DmxInput::validAddress()) {
    ServoEngine::park(LEFT_WING_PARK_US, RIGHT_WING_PARK_US, BEAK_PARK_US);
    if (!AudioEngine::busy()) LedEngine::showAddressError(now);
    wdt_reset();
    return;
  }

  if (DmxInput::update()) {
    const uint8_t receivedMode = DmxInput::value(CHANNEL_MODE);
    // Require every mode, including static mode, to persist for three complete
    // frames. This prevents one damaged mode slot from cancelling an action.
    if (receivedMode == candidateMode) {
      if (candidateFrames < 3) ++candidateFrames;
    } else {
      candidateMode = receivedMode;
      candidateFrames = 1;
    }
    if (candidateFrames >= 3 && receivedMode != stableMode &&
        (receivedMode < 16 || !EffectEngine::modeLocked())) {
      const uint8_t previousMode = stableMode;
      stableMode = receivedMode;
      if (previousMode < 16 && receivedMode >= 16) {
        // Drop any not-yet-transmitted manual request. A packet already on the
        // wire is allowed to finish, but cannot be interrupted safely.
        AudioEngine::cancel();
      }
      if (receivedMode < 16 && EffectEngine::modeLocked()) {
        EffectEngine::cancel(now);
        AudioEngine::cancel();
      }
    }
  }
  const bool dmxTimedOut = DmxInput::timedOut();
  // A logical DMX blackout does not interrupt an action already in progress.
  // The effect owns a latched snapshot until its finite timeline completes.
  if (dmxTimedOut && !EffectEngine::running()) {
    EffectEngine::ServoTargets parked = {LEFT_WING_PARK_US, RIGHT_WING_PARK_US, BEAK_PARK_US};
    if (!timeoutActive) {
      EffectEngine::cancel(now);
      AudioEngine::cancel();
      timeoutActive = true;
    }
    LedEngine::clear();
    updateServos(parked, SERVO_SLEW_MAX_US);
  } else {
    timeoutActive = false;
    EffectEngine::Output output;
    uint8_t channels[DMX_FOOTPRINT];
    for (uint8_t i = 0; i < DMX_FOOTPRINT; ++i) channels[i] = DmxInput::value(i);
    channels[CHANNEL_MODE] = stableMode;

    // Manual sound-channel changes are handled before effects. During a
    // logical blackout, the live DMX frame is intentionally ignored while a
    // latched action finishes.
    if (!dmxTimedOut && stableMode < 16) {
      updateAudio(true);
    }
    EffectEngine::update(channels, now, output);
    const bool presetRunning = EffectEngine::running();
    if (output.soundTrack != 0 && AudioEngine::requestPresetTrack(output.soundTrack))
      EffectEngine::audioRequested();
    // Timer-driven audio TX and servo pulse generation operate concurrently.
    AudioEngine::update(true, stableMode < 16 && !presetRunning);
    if (AudioEngine::takeTrackTransmitted())
      EffectEngine::audioTransmitted(millis());

    updateServos(output.servos, output.servoStepUs);
    // LedEngine retains deferred frames and starts writes only in a safe quiet
    // window between servo pulses and complete audio packets.
    LedEngine::update(output.leds, now);
  }

  wdt_reset();
}
