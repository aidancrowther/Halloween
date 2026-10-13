/*
 * Bird Skeleton Animatronic - production-driver component test.
 *
 * This sketch is isolated from the production entry point and does not
 * initialize or read DMX. Its component drivers are the production
 * ServoEngine, LedEngine, and AudioEngine implementations, included through
 * the bridge files in this directory.
 */

#include <Arduino.h>

#include "AudioEngine.h"
#include "Config.h"
#include "LedEngine.h"
#include "ServoEngine.h"

namespace {

constexpr uint16_t SERVO_TEST_INTERVAL_MS = 25;
constexpr uint16_t LED_TEST_INTERVAL_MS = 1000;
constexpr uint16_t VOLUME_TEST_INTERVAL_MS = 500;
constexpr uint16_t TRACK_TEST_INTERVAL_MS = 10000;
constexpr uint8_t LED_TEST_BRIGHTNESS = 64; // approximately 25% of 255

uint8_t servoAngle = LEFT_WING_MIN_ANGLE;
int8_t servoDirection = 1;
uint8_t volume = 0;
uint8_t ledColor = 0;

uint32_t lastServoTest = 0;
uint32_t lastLedTest = 0;
uint32_t lastVolumeTest = 0;
uint32_t lastTrackTest = 0;

void updateServoTest(uint32_t now)
{
  if (now - lastServoTest < SERVO_TEST_INTERVAL_MS) return;
  lastServoTest = now;

  const uint16_t wingPulse = angleToMicroseconds(servoAngle);
  const uint16_t beakAngle = static_cast<uint16_t>(BEAK_MIN_ANGLE) +
                             (static_cast<uint16_t>(BEAK_MAX_ANGLE - BEAK_MIN_ANGLE) *
                              servoAngle) / 180U;
  const uint16_t beakPulse = angleToMicroseconds(static_cast<uint8_t>(beakAngle));
  ServoEngine::setPulseWidths(wingPulse, wingPulse, beakPulse);

  if (servoDirection > 0) {
    if (servoAngle >= LEFT_WING_MAX_ANGLE) {
      servoDirection = -1;
    } else {
      ++servoAngle;
    }
  } else if (servoAngle <= LEFT_WING_MIN_ANGLE) {
    servoDirection = 1;
  } else {
    --servoAngle;
  }
}

void updateLedTest(uint32_t now)
{
  if (now - lastLedTest < LED_TEST_INTERVAL_MS) return;
  lastLedTest = now;

  LedEngine::Frame frame = {};
  frame.dimmer = LED_TEST_BRIGHTNESS;
  frame.effect = 0; // production LedEngine static RGB mode
  frame.rate = 0;

  if (ledColor == 0) {
    frame.red = 255;
  } else if (ledColor == 1) {
    frame.green = 255;
  } else {
    frame.blue = 255;
  }

  LedEngine::update(frame, now);
  ledColor = static_cast<uint8_t>((ledColor + 1U) % 3U);
}

void updateAudioTest(uint32_t now)
{
  if (now - lastTrackTest >= TRACK_TEST_INTERVAL_MS) {
    // Request track 1, corresponding to the root-level /0001.mp3 file.
    AudioEngine::requestCommand(0x03, 1);
    lastTrackTest = now;
    Serial.println(F("DFPlayer: requested file/track 1"));
  }

  if (now - lastVolumeTest >= VOLUME_TEST_INTERVAL_MS) {
    AudioEngine::setVolume(volume);
    Serial.print(F("DFPlayer: queued volume "));
    Serial.println(volume);
    volume = (volume >= 30) ? 0 : static_cast<uint8_t>(volume + 1);
    lastVolumeTest = now;
  }

  AudioEngine::update(true);
}

}

void setup()
{
  Serial.begin(9600);
  Serial.println(F("Bird Skeleton component test: production drivers, no DMX"));

  ServoEngine::begin(LEFT_WING_PARK_US, RIGHT_WING_PARK_US, BEAK_PARK_US);
  LedEngine::begin();
  AudioEngine::begin();

  lastTrackTest = millis() - TRACK_TEST_INTERVAL_MS;
  lastVolumeTest = millis() - VOLUME_TEST_INTERVAL_MS;
  Serial.println(F("Production ServoEngine, LedEngine, and AudioEngine initialized"));
}

void loop()
{
  const uint32_t now = millis();
  updateServoTest(now);
  updateLedTest(now);
  updateAudioTest(now);
}