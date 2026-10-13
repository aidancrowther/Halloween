#include <Arduino.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>
#include <SoftPWM.h>

SOFTPWM_DEFINE_CHANNEL(0, DDRD, PORTD, PORTD7);
SOFTPWM_DEFINE_CHANNEL(1, DDRD, PORTD, PORTD6);
SOFTPWM_DEFINE_CHANNEL(2, DDRD, PORTD, PORTD5);
SOFTPWM_DEFINE_CHANNEL(3, DDRD, PORTD, PORTD4);
SOFTPWM_DEFINE_CHANNEL(4, DDRD, PORTD, PORTD3);
SOFTPWM_DEFINE_CHANNEL(5, DDRD, PORTD, PORTD2);
SOFTPWM_DEFINE_CHANNEL(6, DDRC, PORTC, PORTC4);
SOFTPWM_DEFINE_CHANNEL(7, DDRC, PORTC, PORTC3);
SOFTPWM_DEFINE_OBJECT_WITH_PWM_LEVELS(8, 256);

static const uint8_t OUTPUT_COUNT = 8;
static const uint8_t DMX_FOOTPRINT = OUTPUT_COUNT;
static const uint16_t MAX_START_ADDRESS = 512 - DMX_FOOTPRINT + 1;
static const uint16_t DMX_TIMEOUT_MS = 1000;
static const uint16_t PWM_FREQUENCY_HZ = 100;
static const uint8_t ADDRESS_B_MASK = _BV(PB1) | _BV(PB2) | _BV(PB3) | _BV(PB4) | _BV(PB5);
static const uint8_t ADDRESS_C_MASK = _BV(PC0) | _BV(PC1) | _BV(PC2);
static const uint8_t STATUS_LED_MASK = _BV(PC5);
static const uint8_t PWM_OUTPUT_MASK_C = _BV(PC3) | _BV(PC4);

static volatile uint8_t staged[DMX_FOOTPRINT] = {0};
static volatile uint8_t published[2][DMX_FOOTPRINT] = {{0}, {0}};
static volatile uint8_t publishedIndex = 0;
static volatile uint8_t frameGeneration = 0;
static volatile uint8_t receiveState = 0;
static volatile uint16_t channelNumber = 0;
static volatile uint32_t lastValidFrameMs = 0;
static volatile bool statusPending = false;
static volatile bool statusReceiverIdle = true;
static volatile bool statusTargetGreen = false;
static uint8_t levels[DMX_FOOTPRINT] = {0};
static uint8_t consumedGeneration = 0;
static uint16_t startAddress = 1;
static bool addressValid = false;
static bool blackoutApplied = false;
static bool statusIsGreen = false;

static uint16_t readStartAddress() {
  DDRB &= (uint8_t)~ADDRESS_B_MASK;
  DDRC &= (uint8_t)~ADDRESS_C_MASK;
  PORTB |= ADDRESS_B_MASK;
  PORTC |= ADDRESS_C_MASK;
  const uint8_t selectedB = (uint8_t)~PINB;
  const uint8_t selectedC = (uint8_t)~PINC;
  uint16_t address = 1;
  if (selectedB & _BV(PB1)) address |= _BV(8);
  if (selectedB & _BV(PB2)) address |= _BV(7);
  if (selectedB & _BV(PB3)) address |= _BV(6);
  if (selectedB & _BV(PB4)) address |= _BV(5);
  if (selectedB & _BV(PB5)) address |= _BV(4);
  if (selectedC & _BV(PC0)) address |= _BV(3);
  if (selectedC & _BV(PC1)) address |= _BV(2);
  if (selectedC & _BV(PC2)) address |= _BV(1);
  return address;
}

static void publishFrame() {
  const uint8_t nextIndex = publishedIndex ^ 1U;
  for (uint8_t i = 0; i < DMX_FOOTPRINT; ++i)
    published[nextIndex][i] = staged[i];
  publishedIndex = nextIndex;
  ++frameGeneration;
}

static void applyLevels(const uint8_t *values) {
  for (uint8_t channel = 0; channel < OUTPUT_COUNT; ++channel)
    Palatis::SoftPWM.set(channel, values[channel]);
}

static void setBlackout() {
  for (uint8_t channel = 0; channel < OUTPUT_COUNT; ++channel) {
    levels[channel] = 0;
    Palatis::SoftPWM.set(channel, 0);
  }
  blackoutApplied = true;
}

// Shared status sender: same PC5 SBI/CBI timing as the WS2812 variant.
static inline void sendStatusBit(bool one) {
  if (one) {
    asm volatile("sbi %[port], %[bit]\n\t .rept 9\n\t nop\n\t .endr\n\t cbi %[port], %[bit]\n\t .rept 7\n\t nop\n\t .endr\n\t"
        : : [port] "I" (_SFR_IO_ADDR(PORTC)), [bit] "I" (PC5) : "memory");
  } else {
    asm volatile("sbi %[port], %[bit]\n\t .rept 3\n\t nop\n\t .endr\n\t cbi %[port], %[bit]\n\t .rept 13\n\t nop\n\t .endr\n\t"
        : : [port] "I" (_SFR_IO_ADDR(PORTC)), [bit] "I" (PC5) : "memory");
  }
}

static void sendStatusByte(uint8_t value) {
  for (uint8_t bit = 0; bit < 8; ++bit) {
    sendStatusBit(value & 0x80);
    value <<= 1;
  }
}

static void writeStatusLed(bool green) {
  const uint8_t savedSREG = SREG;
  cli();
  sendStatusByte(green ? 255 : 0); // G
  sendStatusByte(green ? 0 : 32);  // R
  sendStatusByte(0);               // B
  SREG = savedSREG;
  delayMicroseconds(80);
}

static void serviceStatus() {
  const uint8_t savedSREG = SREG;
  cli();
  const bool pending = statusPending;
  const bool target = statusTargetGreen;
  const bool receiverIdle = statusReceiverIdle;
  if (!pending || !receiverIdle) {
    SREG = savedSREG;
    return;
  }
  statusPending = false;
  statusIsGreen = target;
  SREG = savedSREG;
  writeStatusLed(target);
}

void setup() {
  wdt_disable();
  startAddress = readStartAddress();
  addressValid = startAddress <= MAX_START_ADDRESS;
  DDRC |= STATUS_LED_MASK | PWM_OUTPUT_MASK_C;
  PORTC &= (uint8_t)~STATUS_LED_MASK;
  Palatis::SoftPWM.begin(PWM_FREQUENCY_HZ);
  setBlackout();
  UCSRA = 0;
  UBRRH = 0;
  UBRRL = 3;
  UCSRB = _BV(RXEN) | _BV(RXCIE);
  UCSRC = _BV(URSEL) | _BV(UCSZ1) | _BV(UCSZ0);
  receiveState = 0;
  channelNumber = 0;
  lastValidFrameMs = millis();
  writeStatusLed(false);
}

void loop() {
  uint8_t snapshot[DMX_FOOTPRINT];
  const uint8_t oldSREG = SREG;
  cli();
  const bool hasNewFrame = frameGeneration != consumedGeneration;
  if (hasNewFrame) {
    const uint8_t index = publishedIndex;
    for (uint8_t i = 0; i < DMX_FOOTPRINT; ++i)
      snapshot[i] = published[index][i];
    consumedGeneration = frameGeneration;
  }
  const uint32_t lastFrame = lastValidFrameMs;
  SREG = oldSREG;

  if (statusIsGreen && (uint32_t)(millis() - lastFrame) > DMX_TIMEOUT_MS) {
    const uint8_t statusSREG = SREG;
    cli();
    statusTargetGreen = false;
    statusPending = true;
    statusReceiverIdle = true;
    SREG = statusSREG;
  }
  serviceStatus();

  if (!addressValid) {
    if (!blackoutApplied) setBlackout();
    return;
  }
  if ((uint32_t)(millis() - lastFrame) > DMX_TIMEOUT_MS) {
    if (!blackoutApplied) setBlackout();
    return;
  }
  if (hasNewFrame) {
    for (uint8_t i = 0; i < DMX_FOOTPRINT; ++i) levels[i] = snapshot[i];
    applyLevels(levels);
    blackoutApplied = false;
  }
}

ISR(USART_RXC_vect) {
  const uint8_t status = UCSRA;
  const uint8_t data = UDR;
  if (status & _BV(FE)) {
    receiveState = 1;
    channelNumber = 0;
    statusReceiverIdle = true;
    return;
  }
  if (status & (_BV(DOR) | _BV(PE))) {
    receiveState = 4;
    return;
  }
  if (receiveState == 1) {
    receiveState = data == 0 ? 2 : 0;
    if (data == 0) statusReceiverIdle = false;
    return;
  }
  if (receiveState == 4) return;
  if (receiveState == 3) {
    if (channelNumber < 512) ++channelNumber;
    if (channelNumber >= 512) statusReceiverIdle = true;
    return;
  }
  if (receiveState != 2) return;
  ++channelNumber;
  if (channelNumber >= startAddress && channelNumber < startAddress + DMX_FOOTPRINT)
    staged[channelNumber - startAddress] = data;
  if (channelNumber == startAddress + DMX_FOOTPRINT - 1) {
    receiveState = 3;
    lastValidFrameMs = millis();
    // Allow the coarse status write after this fixture's final slot. Any
    // framing loss in trailing data is recovered at the next DMX break.
    statusReceiverIdle = true;
    if (addressValid && !statusIsGreen) {
      statusTargetGreen = true;
      statusPending = true;
    }
    publishFrame();
  } else if (channelNumber >= 512) {
    receiveState = 0;
    statusReceiverIdle = true;
  }
}
