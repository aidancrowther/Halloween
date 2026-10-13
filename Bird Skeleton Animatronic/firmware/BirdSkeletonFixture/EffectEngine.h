#pragma once

#include <Arduino.h>

#include "LedEngine.h"

namespace EffectEngine {

struct ServoTargets {
  uint16_t left;
  uint16_t right;
  uint16_t beak;
};

struct Output {
  ServoTargets servos;
  LedEngine::Frame leds;
  uint16_t soundTrack;
  uint16_t servoStepUs;
};

void begin();
void cancel(uint32_t now);
void update(const uint8_t *channels, uint32_t now, Output &output);
void audioRequested();
void audioTransmitted(uint32_t now);
bool running();
bool modeLocked();

}
