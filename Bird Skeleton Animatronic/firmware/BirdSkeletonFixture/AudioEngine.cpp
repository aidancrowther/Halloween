#include "AudioEngine.h"

#include "Config.h"
#include "DfPlayerSerial.h"

namespace {

uint8_t pendingCommand = 0;
uint16_t pendingParameter = 0;
bool commandPending = false;
bool trackTransmitted = false;
bool trackInFlight = false;
bool pendingPresetTrack = false;
uint8_t requestedVolume = 0;
uint8_t sentVolume = 255;

bool sendPacket(uint8_t command, uint16_t parameter)
{
  if (DfPlayerSerial::busy()) return false;

  uint8_t packet[10] = {0x7E, 0xFF, 0x06, command, 0x01,
                        static_cast<uint8_t>(parameter >> 8),
                        static_cast<uint8_t>(parameter & 0xFF), 0x00, 0x00, 0xEF};
  uint16_t checksum = 0;
  for (uint8_t i = 1; i < 7; ++i) checksum += packet[i];
  checksum = static_cast<uint16_t>(0U - checksum);
  packet[7] = static_cast<uint8_t>(checksum >> 8);
  packet[8] = static_cast<uint8_t>(checksum & 0xFF);

  if (!DfPlayerSerial::beginPacket(packet, sizeof(packet))) return false;
  return true;
}

}

namespace AudioEngine {

void begin()
{
  DfPlayerSerial::begin();
  pendingCommand = 0;
  pendingParameter = 0;
  commandPending = false;
  trackTransmitted = false;
  trackInFlight = false;
  pendingPresetTrack = false;
  requestedVolume = 0;
  sentVolume = 255;
}

void cancel()
{
  pendingCommand = 0;
  pendingParameter = 0;
  commandPending = false;
  pendingPresetTrack = false;
}

bool requestTrack(uint16_t track)
{
  return track != 0 && requestCommand(0x03, track);
}

bool requestPresetTrack(uint16_t track)
{
  if (track == 0 || commandPending) return false;
  pendingCommand = 0x03;
  pendingParameter = track;
  pendingPresetTrack = true;
  commandPending = true;
  return true;
}

bool requestCommand(uint8_t command, uint16_t parameter)
{
  if (commandPending) return false;
  pendingCommand = command;
  pendingParameter = parameter;
  pendingPresetTrack = false;
  commandPending = true;
  return true;
}

void setVolume(uint8_t volume)
{
  requestedVolume = volume > 30 ? 30 : volume;
}

bool update(bool allowStart, bool allowManualAudio)
{
  if (trackInFlight && DfPlayerSerial::completed()) {
    trackInFlight = false;
    trackTransmitted = true;
  }
  if (commandPending && allowStart && (pendingPresetTrack || allowManualAudio) &&
      sendPacket(pendingCommand, pendingParameter)) {
    trackInFlight = pendingPresetTrack;
    pendingPresetTrack = false;
    commandPending = false;
  }
  if (allowManualAudio && !commandPending && !trackInFlight &&
      requestedVolume != sentVolume && allowStart &&
      sendPacket(0x06, requestedVolume)) {
    sentVolume = requestedVolume;
  }
  return false;
}

bool takeTrackTransmitted()
{
  const bool result = trackTransmitted;
  trackTransmitted = false;
  return result;
}

bool busy()
{
  // LEDs may use a servo quiet window between complete packets.
  return DfPlayerSerial::busy();
}

}
