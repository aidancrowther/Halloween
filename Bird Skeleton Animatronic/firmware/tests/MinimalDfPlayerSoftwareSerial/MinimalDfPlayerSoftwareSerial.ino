/*
 * Minimal ATmega8 DFPlayer test using MiniCore's standard SoftwareSerial.
 *
 * ATmega8 wiring:
 *   PD6 / Arduino D6  -> DFPlayer RX
 *   PD2 / Arduino D2  <- DFPlayer TX (ATmega8 INT0; required for RX)
 *   GND               -> DFPlayer GND
 *
 * USB serial diagnostics use the ATmega8 hardware UART at 115200 baud.
 * No fixture components are initialized by this test.
 */

#include <Arduino.h>
#include <SoftwareSerial.h>

namespace {

constexpr uint8_t DFPLAYER_RX_PIN = 2;
constexpr uint8_t DFPLAYER_TX_PIN = 6;
constexpr uint32_t DFPLAYER_BAUD = 9600UL;
constexpr uint8_t DFPLAYER_MAX_VOLUME = 30;
constexpr uint32_t COMMAND_INTERVAL_MS = 5000UL;

SoftwareSerial dfPlayer(DFPLAYER_RX_PIN, DFPLAYER_TX_PIN);
uint8_t responseFrame[10] = {};
uint8_t responseIndex = 0;
uint32_t lastCommand = 0;

void printHexByte(uint8_t value)
{
  if (value < 0x10) Serial.print('0');
  Serial.print(value, HEX);
}

void sendCommand(uint8_t command, uint16_t parameter)
{
  uint8_t packet[10] = {
    0x7E, 0xFF, 0x06, command, 0x01,
    static_cast<uint8_t>(parameter >> 8),
    static_cast<uint8_t>(parameter & 0xFF),
    0x00, 0x00, 0xEF
  };

  uint16_t checksum = 0;
  for (uint8_t i = 1; i < 7; ++i) checksum += packet[i];
  checksum = static_cast<uint16_t>(0U - checksum);
  packet[7] = static_cast<uint8_t>(checksum >> 8);
  packet[8] = static_cast<uint8_t>(checksum & 0xFF);

  Serial.print(F("TX command=0x"));
  printHexByte(command);
  Serial.print(F(" parameter=0x"));
  printHexByte(static_cast<uint8_t>(parameter >> 8));
  printHexByte(static_cast<uint8_t>(parameter & 0xFF));
  Serial.println();

  dfPlayer.write(packet, sizeof(packet));
  dfPlayer.flush();
}

bool validResponse()
{
  if (responseFrame[0] != 0x7E || responseFrame[1] != 0xFF ||
      responseFrame[2] != 0x06 || responseFrame[9] != 0xEF) {
    return false;
  }

  uint16_t checksum = 0;
  for (uint8_t i = 1; i < 7; ++i) checksum += responseFrame[i];
  checksum = static_cast<uint16_t>(0U - checksum);
  return responseFrame[7] == static_cast<uint8_t>(checksum >> 8) &&
         responseFrame[8] == static_cast<uint8_t>(checksum & 0xFF);
}

void readResponses()
{
  while (dfPlayer.available()) {
    const uint8_t byte = static_cast<uint8_t>(dfPlayer.read());

    if (responseIndex == 0 && byte != 0x7E) continue;
    responseFrame[responseIndex++] = byte;

    if (responseIndex >= sizeof(responseFrame)) {
      Serial.print(F("RX frame: "));
      for (uint8_t i = 0; i < sizeof(responseFrame); ++i) {
        printHexByte(responseFrame[i]);
        if (i + 1 < sizeof(responseFrame)) Serial.print(' ');
      }
      Serial.print(F(" valid="));
      Serial.println(validResponse() ? F("yes") : F("no"));
      responseIndex = 0;
    }
  }
}

}

void setup()
{
  Serial.begin(115200);
  Serial.println(F("Minimal DFPlayer SoftwareSerial test starting"));
  Serial.println(F("DFPlayer RX: ATmega8 PD2/D2; DFPlayer TX: ATmega8 PD6/D6"));
  dfPlayer.begin(DFPLAYER_BAUD);

  // Allow the DFPlayer to finish its power-up sequence.
  delay(1000);

  // Reset, set maximum volume, then request file/track number 1.
  sendCommand(0x0C, 0);
  delay(1000);
  sendCommand(0x06, DFPLAYER_MAX_VOLUME);
  delay(250);
  sendCommand(0x03, 1);
  lastCommand = millis();
}

void loop()
{
  readResponses();

  const uint32_t now = millis();
  if (now - lastCommand >= COMMAND_INTERVAL_MS) {
    sendCommand(0x06, DFPLAYER_MAX_VOLUME);
    delay(100);
    sendCommand(0x03, 1);
    lastCommand = millis();
  }
}