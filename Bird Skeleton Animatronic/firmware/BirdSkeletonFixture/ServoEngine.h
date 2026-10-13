#pragma once

#include <Arduino.h>

namespace ServoEngine {

void begin(uint16_t leftUs, uint16_t rightUs, uint16_t beakUs);
void setPulseWidths(uint16_t leftUs, uint16_t rightUs, uint16_t beakUs);
void park(uint16_t leftUs, uint16_t rightUs, uint16_t beakUs);
bool inQuietPeriod();
bool canBlockFor(uint16_t durationUs);

}
