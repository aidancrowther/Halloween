#pragma once

#include <Arduino.h>

namespace LedEngine {

struct Frame {
  uint8_t red;
  uint8_t green;
  uint8_t blue;
  uint8_t dimmer;
  uint8_t effect;
  uint8_t rate;
};

void begin();
void update(const Frame &frame, uint32_t now);
void clear();
void showAddressError(uint32_t now);
bool canTransmitNow();

}
