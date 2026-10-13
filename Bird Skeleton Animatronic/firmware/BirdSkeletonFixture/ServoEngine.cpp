#include "ServoEngine.h"

#include <avr/interrupt.h>
#include <avr/io.h>

#include "Config.h"

namespace {

volatile uint16_t pulseUs[3] = {544, 544, 544};
volatile uint16_t framePulseUs[3] = {544, 544, 544};
volatile uint8_t sequenceStep = 0;
volatile bool quietPeriod = false;

constexpr uint8_t LEFT_MASK = _BV(PD2);
constexpr uint8_t RIGHT_MASK = _BV(PD3);
constexpr uint8_t BEAK_MASK = _BV(PD4);
constexpr uint16_t TICKS_PER_US = 2; // Timer1 prescaler 8 at 16 MHz

inline uint16_t pulseTicks(uint16_t us)
{
  return us * TICKS_PER_US;
}

inline void schedulePulse(uint16_t us)
{
  // Timer1 runs in normal mode. Schedule relative to the current counter so
  // every edge remains correct regardless of the previous pulse duration.
  OCR1A = static_cast<uint16_t>(TCNT1 + pulseTicks(us));
}

}

namespace ServoEngine {

void begin(uint16_t leftUs, uint16_t rightUs, uint16_t beakUs)
{
  DDRD |= LEFT_MASK | RIGHT_MASK | BEAK_MASK;
  PORTD &= static_cast<uint8_t>(~(LEFT_MASK | RIGHT_MASK | BEAK_MASK));

  setPulseWidths(leftUs, rightUs, beakUs);

  // Timer1 normal mode, prescaler /8: 0.5 us per timer tick. Its interrupt
  // remains active during Timer2 DFPlayer packet transmission.
  TCCR1A = 0;
  TCCR1B = _BV(CS11);
  TCNT1 = 0;
  OCR1A = 1;
  sequenceStep = 0;
  quietPeriod = false;
  TIMSK |= _BV(OCIE1A);
}

void setPulseWidths(uint16_t leftUs, uint16_t rightUs, uint16_t beakUs)
{
  if (leftUs > SERVO_FRAME_US) leftUs = SERVO_FRAME_US;
  if (rightUs > SERVO_FRAME_US) rightUs = SERVO_FRAME_US;
  if (beakUs > SERVO_FRAME_US) beakUs = SERVO_FRAME_US;
  uint8_t oldSreg = SREG;
  cli();
  pulseUs[0] = leftUs;
  pulseUs[1] = rightUs;
  pulseUs[2] = beakUs;
  SREG = oldSreg;
}

void park(uint16_t leftUs, uint16_t rightUs, uint16_t beakUs)
{
  setPulseWidths(leftUs, rightUs, beakUs);
}

bool inQuietPeriod()
{
  return quietPeriod;
}

bool canBlockFor(uint16_t durationUs)
{
  uint8_t oldSreg = SREG;
  cli();
  const uint16_t remainingTicks = static_cast<uint16_t>(OCR1A - TCNT1);
  const bool safe = quietPeriod && remainingTicks < 0x8000U &&
      remainingTicks > static_cast<uint32_t>(durationUs + 40U) * TICKS_PER_US;
  SREG = oldSreg;
  return safe;
}

}

ISR(TIMER1_COMPA_vect)
{
  switch (sequenceStep) {
    case 0:
      quietPeriod = false;
      framePulseUs[0] = pulseUs[0];
      framePulseUs[1] = pulseUs[1];
      framePulseUs[2] = pulseUs[2];
      PORTD |= _BV(PD2);
      schedulePulse(framePulseUs[0]);
      sequenceStep = 1;
      break;

    case 1:
      PORTD &= static_cast<uint8_t>(~_BV(PD2));
      PORTD |= _BV(PD3);
      schedulePulse(framePulseUs[1]);
      sequenceStep = 2;
      break;

    case 2:
      PORTD &= static_cast<uint8_t>(~_BV(PD3));
      PORTD |= _BV(PD4);
      schedulePulse(framePulseUs[2]);
      sequenceStep = 3;
      break;

    default: {
      PORTD &= static_cast<uint8_t>(~_BV(PD4));
      uint32_t usedUs = static_cast<uint32_t>(framePulseUs[0]) +
          framePulseUs[1] + framePulseUs[2];
      uint16_t quietUs = (usedUs < SERVO_REFRESH_US)
          ? static_cast<uint16_t>(SERVO_REFRESH_US - usedUs) : 1;
      schedulePulse(quietUs);
      quietPeriod = true;
      sequenceStep = 0;
      break;
    }
  }
}
