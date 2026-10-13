#pragma once

#include <Arduino.h>

namespace AudioEngine {

void begin();
void cancel();
// Preset packets may start while manual audio is suppressed. An already-started
// packet always completes, regardless of these gates.
bool update(bool allowStart = true, bool allowManualAudio = true);
bool takeTrackTransmitted();
bool requestTrack(uint16_t track);
bool requestPresetTrack(uint16_t track);
bool requestCommand(uint8_t command, uint16_t parameter = 0);
void setVolume(uint8_t volume);
bool busy();

}
