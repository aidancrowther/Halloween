#pragma once

#include <Arduino.h>

namespace DfPlayerSerial {

void begin();
bool beginPacket(const uint8_t *data, uint8_t length);
bool busy();
bool completed();

}