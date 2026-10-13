#include "DfPlayerSerial.h"

#include <avr/interrupt.h>
#include <avr/io.h>

#include "Config.h"
#include "DmxInput.h"

namespace {

constexpr uint8_t TX_MASK = _BV(PD6);
constexpr uint8_t PACKET_BUFFER_SIZE = 10;

volatile uint8_t packet[PACKET_BUFFER_SIZE] = {};
volatile uint8_t packetLength = 0;
volatile uint8_t packetByte = 0;
volatile uint8_t packetBit = 0;
volatile bool transmitting = false;
volatile bool finishingStopBit = false;
volatile bool packetCompleted = false;

void configureTimer()
{
  // Timer2 CTC, /8 prescaler: 16 MHz / 8 / (207 + 1) = 9615 baud.
  TCCR2 = _BV(WGM21) | _BV(CS21);
  OCR2 = 207;
  TIMSK &= static_cast<uint8_t>(~_BV(OCIE2));
  TIFR |= _BV(OCF2);
}

inline void emitCurrentBit()
{
  if (packetBit == 0) {
    PORTD &= static_cast<uint8_t>(~TX_MASK);
    packetBit = 1;
  } else if (packetBit <= 8) {
    if (packet[packetByte] & _BV(packetBit - 1U)) PORTD |= TX_MASK;
    else PORTD &= static_cast<uint8_t>(~TX_MASK);
    ++packetBit;
  } else {
    PORTD |= TX_MASK;
    ++packetByte;
    if (packetByte >= packetLength) finishingStopBit = true;
    else packetBit = 0;
  }
}

}

namespace DfPlayerSerial {

void begin()
{
  uint8_t oldSreg = SREG;
  cli();
  DDRD |= TX_MASK;
  PORTD |= TX_MASK;
  configureTimer();
  SREG = oldSreg;
}

bool beginPacket(const uint8_t *data, uint8_t length)
{
  if (data == nullptr || length == 0 || length > PACKET_BUFFER_SIZE) return false;

  uint8_t oldSreg = SREG;
  cli();
  if (transmitting) {
    SREG = oldSreg;
    return false;
  }

  for (uint8_t i = 0; i < length; ++i) packet[i] = data[i];
  packetLength = length;
  packetByte = 0;
  packetBit = 0;
  transmitting = true;
  finishingStopBit = false;
  packetCompleted = false;
  PORTD |= TX_MASK;
  DmxInput::pauseReception();
  // TX is timer-driven on PD6. The UART RX interrupt is paused for the packet.
  emitCurrentBit();
  TIFR |= _BV(OCF2);
  TIMSK |= _BV(OCIE2);
  SREG = oldSreg;
  return true;
}

bool busy()
{
  return transmitting;
}

bool completed()
{
  uint8_t oldSreg = SREG;
  cli();
  const bool result = packetCompleted;
  packetCompleted = false;
  SREG = oldSreg;
  return result;
}

}

ISR(TIMER2_COMP_vect)
{
  if (finishingStopBit) {
    finishingStopBit = false;
    transmitting = false;
    TIMSK &= static_cast<uint8_t>(~_BV(OCIE2));
    packetLength = 0;
    packetByte = 0;
    packetBit = 0;
    PORTD |= TX_MASK;
    packetCompleted = true;
    DmxInput::resumeReception();
    return;
  }

  if (!transmitting) {
    TIMSK &= static_cast<uint8_t>(~_BV(OCIE2));
    PORTD |= _BV(PD6);
    return;
  }

  emitCurrentBit();
}