#include <Arduino.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>

// Parallel WS2812 outputs: eight independently controlled bit streams.
static const uint8_t DATA_D_MASK = 0xFC; // PD2..PD7
static const uint8_t DATA_C_MASK = _BV(PC3) | _BV(PC4);
static const uint8_t SEGMENT_COUNT = 8;
static const uint8_t LEDS_PER_SEGMENT = 7;
static const uint8_t DMX_FOOTPRINT = SEGMENT_COUNT * 3;
static const uint8_t ADDRESS_B_MASK = _BV(PB1) | _BV(PB2) | _BV(PB3) | _BV(PB4) | _BV(PB5);
static const uint8_t ADDRESS_C_MASK = _BV(PC0) | _BV(PC1) | _BV(PC2);
static const uint8_t STATUS_LED_MASK = _BV(PC5);
static const uint16_t DMX_TIMEOUT_MS = 1000;

static volatile uint8_t staged[DMX_FOOTPRINT];
static volatile uint8_t live[DMX_FOOTPRINT];
static volatile uint8_t generation = 0;
static uint8_t consumedGeneration = 0;
static uint8_t nextSegment = 0;
static volatile uint8_t rxState = 0;
static volatile uint16_t channelNumber = 0;
static volatile uint32_t lastValidFrameMs = 0;
static volatile bool statusPending = false;
static volatile bool statusReceiverIdle = true;
static volatile bool statusTargetGreen = false;
static uint16_t startAddress = 1;
static bool addressValid = false;
static uint8_t portCIdle;
static uint8_t portDIdle;
static volatile bool statusIsGreen = false;

static uint16_t readStartAddress() {
  DDRB &= (uint8_t)~ADDRESS_B_MASK;
  DDRC &= (uint8_t)~ADDRESS_C_MASK;
  PORTB |= ADDRESS_B_MASK;
  PORTC |= ADDRESS_C_MASK;
  const uint8_t b = (uint8_t)~PINB;
  const uint8_t c = (uint8_t)~PINC;
  uint16_t address = 1;
  if (b & _BV(PB1)) address |= _BV(8);
  if (b & _BV(PB2)) address |= _BV(7);
  if (b & _BV(PB3)) address |= _BV(6);
  if (b & _BV(PB4)) address |= _BV(5);
  if (b & _BV(PB5)) address |= _BV(4);
  if (c & _BV(PC0)) address |= _BV(3);
  if (c & _BV(PC1)) address |= _BV(2);
  if (c & _BV(PC2)) address |= _BV(1);
  return address;
}

static inline void sendBit(bool one, uint8_t highD, uint8_t highC,
                           uint8_t lowD, uint8_t lowC) {
  if (one) {
    asm volatile("out %[pd], %[hd]\n\t out %[pc], %[hc]\n\t .rept 10\n\t nop\n\t .endr\n\t out %[pd], %[ld]\n\t out %[pc], %[lc]\n\t .rept 6\n\t nop\n\t .endr\n\t"
        : : [pd] "I" (_SFR_IO_ADDR(PORTD)), [pc] "I" (_SFR_IO_ADDR(PORTC)),
            [hd] "r" (highD), [hc] "r" (highC), [ld] "r" (lowD), [lc] "r" (lowC) : "memory");
  } else {
    asm volatile("out %[pd], %[hd]\n\t out %[pc], %[hc]\n\t .rept 4\n\t nop\n\t .endr\n\t out %[pd], %[ld]\n\t out %[pc], %[lc]\n\t .rept 12\n\t nop\n\t .endr\n\t"
        : : [pd] "I" (_SFR_IO_ADDR(PORTD)), [pc] "I" (_SFR_IO_ADDR(PORTC)),
            [hd] "r" (highD), [hc] "r" (highC), [ld] "r" (lowD), [lc] "r" (lowC) : "memory");
  }
}

static inline void sendByte(uint8_t value, uint8_t highD, uint8_t highC,
                            uint8_t lowD, uint8_t lowC) {
  for (uint8_t bit = 0; bit < 8; ++bit) {
    sendBit(value & 0x80, highD, highC, lowD, lowC);
    value <<= 1;
  }
}

// Shared status sender: PC5 only; SBI/CBI leave all other PORTC pins untouched.
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

static void showSegment(uint8_t segment) {
  const uint8_t savedSREG = SREG;
  cli();
  uint8_t highD = portDIdle;
  uint8_t highC = portCIdle;
  if (segment < 6) highD |= _BV(7 - segment);
  else highC |= _BV(segment == 6 ? PC4 : PC3);
  const uint8_t lowD = portDIdle;
  const uint8_t lowC = portCIdle;
  for (uint8_t led = 0; led < LEDS_PER_SEGMENT; ++led) {
    const uint8_t offset = segment * 3;
    sendByte(live[offset + 1], highD, highC, lowD, lowC);
    sendByte(live[offset], highD, highC, lowD, lowC);
    sendByte(live[offset + 2], highD, highC, lowD, lowC);
  }
  PORTD = portDIdle;
  PORTC = portCIdle;
  SREG = savedSREG;
  delayMicroseconds(80);
}

static void configureDmxReceiver() {
  UCSRA = 0;
  UBRRH = 0;
  UBRRL = 3;
  UCSRB = _BV(RXEN) | _BV(RXCIE);
  UCSRC = _BV(URSEL) | _BV(UCSZ1) | _BV(UCSZ0);
  rxState = 0;
  channelNumber = 0;
}

void setup() {
  wdt_disable();
  DDRC |= STATUS_LED_MASK;
  PORTC &= (uint8_t)~STATUS_LED_MASK;
  DDRD |= DATA_D_MASK;
  startAddress = readStartAddress();
  portDIdle = PORTD & (uint8_t)~DATA_D_MASK;
  PORTD = portDIdle;
  portCIdle = PORTC & (uint8_t)~DATA_C_MASK;
  PORTC = portCIdle;
  DDRC |= DATA_C_MASK;
  addressValid = startAddress <= 489;
  configureDmxReceiver();
  lastValidFrameMs = millis();
  writeStatusLed(false);
  delay(100);
  if (addressValid) showSegment(0);
}

void loop() {
  const uint8_t savedSREG = SREG;
  cli();
  const bool changed = generation != consumedGeneration;
  if (changed) {
    for (uint8_t i = 0; i < DMX_FOOTPRINT; ++i) live[i] = staged[i];
    consumedGeneration = generation;
  }
  const uint32_t lastFrame = lastValidFrameMs;
  SREG = savedSREG;

  if (statusIsGreen && (uint32_t)(millis() - lastFrame) > DMX_TIMEOUT_MS) {
    const uint8_t statusSREG = SREG;
    cli();
    statusTargetGreen = false;
    statusPending = true;
    statusReceiverIdle = true;
    SREG = statusSREG;
  }
  serviceStatus();

  if (changed && addressValid) {
    showSegment(nextSegment);
    nextSegment = (nextSegment + 1) % SEGMENT_COUNT;
  }
}

ISR(USART_RXC_vect) {
  const uint8_t status = UCSRA;
  const uint8_t data = UDR;
  if (status & _BV(FE)) {
    rxState = 1;
    channelNumber = 0;
    statusReceiverIdle = true;
    return;
  }
  if (status & (_BV(DOR) | _BV(PE))) {
    rxState = 4;
    return;
  }
  if (rxState == 1) {
    rxState = data == 0 ? 2 : 0;
    if (data == 0) statusReceiverIdle = false;
    return;
  }
  if (rxState == 4) return;
  if (rxState == 3) {
    if (channelNumber < 512) ++channelNumber;
    if (channelNumber >= 512) statusReceiverIdle = true;
    return;
  }
  if (rxState != 2) return;

  ++channelNumber;
  if (channelNumber >= startAddress && channelNumber < startAddress + DMX_FOOTPRINT)
    staged[channelNumber - startAddress] = data;
  if (channelNumber == startAddress + DMX_FOOTPRINT - 1) {
    rxState = 3;
    lastValidFrameMs = millis();
    // Allow the coarse status write after this fixture's final slot. Any
    // framing loss in trailing data is recovered at the next DMX break.
    statusReceiverIdle = true;
    if (addressValid && !statusIsGreen) {
      statusTargetGreen = true;
      statusPending = true;
    }
    ++generation;
  } else if (channelNumber >= 512) {
    rxState = 0;
    statusReceiverIdle = true;
  }
}
