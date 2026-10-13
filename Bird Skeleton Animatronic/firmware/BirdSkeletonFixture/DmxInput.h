#pragma once

#include <Arduino.h>

namespace DmxInput {

void configureAddressPins();
void begin();
void pauseReception();
void resumeReception();
bool update();
bool validAddress();
bool timedOut();
uint16_t startAddress();
uint8_t value(uint8_t channelOffset);

}
