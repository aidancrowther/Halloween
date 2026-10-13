/*
 * Direct DMXSerial component test for the ATmega8 fixture.
 *
 * This sketch is compiled with DMX_USE_PORT1 so DMXSerial uses the Mega's
 * hardware UART1.  UART1 TX is Arduino Mega pin D18 (TX1), connected directly
 * to the fixture's DMX receive input for this bench test.
 *
 * The fixture is expected to use DMX start address 289. The test exercises
 * track 1, a fixed volume, static servo positions, and RGB LED output.
 */

#include <Arduino.h>
#include <DMXSerial.h>

namespace {

// Address contributions selected on the fixture: 256 + 32, plus the fixed
// least-significant address bit of 1.
constexpr uint16_t DMX_START_CHANNEL = 289;
constexpr uint16_t STAGE_DURATION_MS = 5000;
constexpr uint16_t UPDATE_INTERVAL_MS = 20;
constexpr uint8_t TEST_VOLUME = 192; // approximately 22/30 on the fixture

static_assert(DMX_START_CHANNEL >= 1, "DMX channel numbers are 1-based");
static_assert(DMX_START_CHANNEL + 12 <= 512, "Fixture footprint exceeds DMX range");

enum Stage : uint8_t {
  STAGE_LEFT_SERVO,
  STAGE_RIGHT_SERVO,
  STAGE_BEAK_SERVO,
  STAGE_RED_LED,
  STAGE_GREEN_LED,
  STAGE_BLUE_LED,
  STAGE_PLAY_TRACK,
  STAGE_NEUTRAL,
  STAGE_COUNT
};

uint32_t stageStarted = 0;
uint32_t lastUpdate = 0;
Stage stage = STAGE_LEFT_SERVO;

void writeFixtureChannels(uint8_t left, uint8_t right, uint8_t beak,
                          uint8_t dimmer, uint8_t red, uint8_t green,
                           uint8_t blue, uint8_t sound, uint8_t volume)
{
  // DMXSerial channel numbers are 1-based. The values below may be zero.
  DMXSerial.write(DMX_START_CHANNEL + 1, 0);      // speed
  DMXSerial.write(DMX_START_CHANNEL + 2, left);   // left wing
  DMXSerial.write(DMX_START_CHANNEL + 3, right);  // right wing
  DMXSerial.write(DMX_START_CHANNEL + 4, beak);   // beak
  DMXSerial.write(DMX_START_CHANNEL + 5, dimmer); // dimmer
  DMXSerial.write(DMX_START_CHANNEL + 6, red);    // red
  DMXSerial.write(DMX_START_CHANNEL + 7, green);  // green
  DMXSerial.write(DMX_START_CHANNEL + 8, blue);   // blue
  DMXSerial.write(DMX_START_CHANNEL + 9, 0);      // LED effect
  DMXSerial.write(DMX_START_CHANNEL + 10, 0);     // effect rate
  DMXSerial.write(DMX_START_CHANNEL + 11, sound); // sound/track
  DMXSerial.write(DMX_START_CHANNEL + 12, volume); // volume
}

uint8_t cycleValue(uint32_t elapsed)
{
  if (elapsed >= STAGE_DURATION_MS / 2U) {
    const uint32_t downElapsed = elapsed - STAGE_DURATION_MS / 2U;
    return static_cast<uint8_t>(255U - ((downElapsed * 255UL) / (STAGE_DURATION_MS / 2U)));
  }
  return static_cast<uint8_t>((elapsed * 255UL) / (STAGE_DURATION_MS / 2U));
}

void updateStage(uint32_t now)
{
  const uint32_t elapsed = now - stageStarted;
  const uint8_t value = cycleValue(elapsed);

  uint8_t left = 0;
  uint8_t right = 0;
  uint8_t beak = 0;
  uint8_t dimmer = 0;
  uint8_t red = 0;
  uint8_t green = 0;
  uint8_t blue = 0;
  uint8_t sound = 0;
  const uint8_t volume = TEST_VOLUME;

  switch (stage) {
    case STAGE_LEFT_SERVO:
      left = value;
      break;
    case STAGE_RIGHT_SERVO:
      right = value;
      break;
    case STAGE_BEAK_SERVO:
      beak = value;
      break;
    case STAGE_RED_LED:
      dimmer = 255;
      red = value;
      break;
    case STAGE_GREEN_LED:
      dimmer = 255;
      green = value;
      break;
    case STAGE_BLUE_LED:
      dimmer = 255;
      blue = value;
      break;
    case STAGE_PLAY_TRACK:
      // Sound values 1-200 select the corresponding DFPlayer track.
      sound = 1;
      break;
    case STAGE_NEUTRAL:
      break;
    case STAGE_COUNT:
      break;
  }

  DMXSerial.write(DMX_START_CHANNEL, 0); // channel 1: basic static fixture mode
  writeFixtureChannels(left, right, beak, dimmer, red, green, blue, sound, volume);
}

}

void setup()
{
  DMXSerial.init(DMXController);
  DMXSerial.maxChannel(DMX_START_CHANNEL + 12);
  stageStarted = millis();
}

void loop()
{
  const uint32_t now = millis();

  if (now - stageStarted >= STAGE_DURATION_MS) {
    stage = static_cast<Stage>((static_cast<uint8_t>(stage) + 1U) % STAGE_COUNT);
    stageStarted = now;
  }

  if (now - lastUpdate >= UPDATE_INTERVAL_MS) {
    lastUpdate = now;
    updateStage(now);
  }
}