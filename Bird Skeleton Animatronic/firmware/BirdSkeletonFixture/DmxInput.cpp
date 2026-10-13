#include "DmxInput.h"

#include <avr/interrupt.h>
#include <avr/io.h>

#include "Config.h"

namespace {

uint8_t values[DMX_FOOTPRINT] = {0};
uint16_t address = 1;
bool addressIsValid = false;
volatile uint8_t receiveState = 0;
volatile uint16_t channelNumber = 0;
volatile bool receptionPaused = false;
volatile uint8_t staging[DMX_FOOTPRINT];
volatile uint8_t published[2][DMX_FOOTPRINT];
volatile uint8_t publishedIndex = 0;
volatile uint8_t frameGeneration = 0;
uint8_t consumedGeneration = 0;
volatile uint32_t lastValidFrame = 0;

const uint8_t portBMask = _BV(PB1) | _BV(PB2) | _BV(PB3) | _BV(PB4) | _BV(PB5);
const uint8_t portCMask = _BV(PC0) | _BV(PC1) | _BV(PC2);

void configureAddressPinsInternal()
{
  // Set DDR to input before enabling pull-ups. This ordering prevents these
  // pins from ever being driven as outputs during firmware startup.
  DDRB &= static_cast<uint8_t>(~portBMask);
  DDRC &= static_cast<uint8_t>(~portCMask);
  PORTB |= portBMask;
  PORTC |= portCMask;
}

inline void publishSnapshot()
{
  uint8_t next = publishedIndex ^ 1U;
  for (uint8_t i = 0; i < DMX_FOOTPRINT; ++i) published[next][i] = staging[i];
  publishedIndex = next;
  ++frameGeneration;
}

uint16_t decodeAddress()
{
  configureAddressPinsInternal();

  const uint8_t portBSelected = static_cast<uint8_t>((~PINB) & portBMask);
  const uint8_t portCSelected = static_cast<uint8_t>((~PINC) & portCMask);

  // The physical DIP switch is wired in the opposite order from the logical
  // address numbering. Reverse the GPIO sequence while preserving active-low
  // switch behavior:
  // PB1..PB5, PC0..PC2 -> logical bits 8..4, 3..1.
  uint16_t result = 1;
  if (portBSelected & _BV(PB1)) result |= _BV(8);
  if (portBSelected & _BV(PB2)) result |= _BV(7);
  if (portBSelected & _BV(PB3)) result |= _BV(6);
  if (portBSelected & _BV(PB4)) result |= _BV(5);
  if (portBSelected & _BV(PB5)) result |= _BV(4);
  if (portCSelected & _BV(PC0)) result |= _BV(3);
  if (portCSelected & _BV(PC1)) result |= _BV(2);
  if (portCSelected & _BV(PC2)) result |= _BV(1);
  return result;
}

}

namespace DmxInput {

void configureAddressPins()
{
  configureAddressPinsInternal();
}

void begin()
{
  address = decodeAddress();
  addressIsValid = address <= (512U - DMX_FOOTPRINT + 1U);

  pinMode(DMX_MODE_PIN, OUTPUT);
  digitalWrite(DMX_MODE_PIN, LOW);
  // Receive DMX as 250 kbaud, 8N1. The FE bit marks the DMX break; DOR/UPE
  // invalidate the in-progress frame rather than shifting channel alignment.
  UCSRA = 0;
  UBRRH = 0;
  UBRRL = 3;
  UCSRB = _BV(RXEN) | _BV(RXCIE);
  UCSRC = _BV(URSEL) | _BV(UCSZ1) | _BV(UCSZ0);
  receiveState = 0;
  channelNumber = 0;
  receptionPaused = false;
  lastValidFrame = millis();
  frameGeneration = 0;
  consumedGeneration = 0;
}

void pauseReception()
{
  uint8_t oldSreg = SREG;
  cli();
  receptionPaused = true;
  // Disable the receiver to discard any in-flight byte and clear its receive
  // buffer. Resume starts from a clean hardware/parser boundary.
  UCSRB &= static_cast<uint8_t>(~(_BV(RXCIE) | _BV(RXEN)));
  SREG = oldSreg;
}

void resumeReception()
{
  uint8_t oldSreg = SREG;
  cli();
  // Clear pending receive/error flags with AVR write-one-to-clear semantics,
  // drain any buffered byte, then wait for the next framing-error break.
  UCSRA = _BV(FE) | _BV(DOR) | _BV(PE);
  (void)UDR;
  receiveState = 0;
  channelNumber = 0;
  receptionPaused = false;
  UCSRB |= _BV(RXCIE) | _BV(RXEN);
  SREG = oldSreg;
}

bool update()
{
  uint8_t oldSreg = SREG;
  cli();
  bool updated = frameGeneration != consumedGeneration;
  uint8_t index = publishedIndex;
  for (uint8_t i = 0; i < DMX_FOOTPRINT; ++i) values[i] = published[index][i];
  consumedGeneration = frameGeneration;
  SREG = oldSreg;
  return updated;
}

bool validAddress() { return addressIsValid; }
bool timedOut()
{
  uint8_t oldSreg = SREG;
  cli();
  uint32_t received = lastValidFrame;
  SREG = oldSreg;
  return static_cast<uint32_t>(millis() - received) > DMX_TIMEOUT_MS;
}
uint16_t startAddress() { return address; }
uint8_t value(uint8_t channelOffset)
{
  return channelOffset < DMX_FOOTPRINT ? values[channelOffset] : 0;
}

ISR(USART_RXC_vect)
{
  const uint8_t status = UCSRA;
  const uint8_t data = UDR;
  if (receptionPaused) return;
  if (status & _BV(FE)) {
    // A break is a hard frame boundary. Do not publish a candidate when the
    // UART reported overrun/parity damage anywhere in that universe.
    if (!(status & (_BV(DOR) | _BV(PE))) && receiveState == 3) {
      publishSnapshot();
      lastValidFrame = millis();
    }
    receiveState = 1;
    channelNumber = 0;
    return;
  }
  if (status & (_BV(DOR) | _BV(PE))) {
    receiveState = 4;
    return;
  }
  if (receiveState == 1) {
    receiveState = data == 0 ? 2 : 0;
    return;
  }
  if (receiveState == 4) return;
  if (receiveState == 3) {
    if (channelNumber < 512U) ++channelNumber;
    return;
  }
  if (receiveState != 2) return;

  ++channelNumber;
  if (channelNumber >= address && channelNumber < address + DMX_FOOTPRINT)
    staging[channelNumber - address] = data;
  if (channelNumber == address + DMX_FOOTPRINT - 1U) {
    receiveState = 3;
  } else if (channelNumber >= 512) {
    receiveState = 0;
  }
}

}
