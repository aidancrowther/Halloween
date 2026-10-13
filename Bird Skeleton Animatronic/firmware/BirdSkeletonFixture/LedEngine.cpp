#include "LedEngine.h"

#include <avr/interrupt.h>
#include <avr/io.h>
#include <util/delay.h>

#include "Config.h"
#include "AudioEngine.h"
#include "DmxInput.h"
#include "ServoEngine.h"

namespace {

struct Pixel { uint8_t red; uint8_t green; uint8_t blue; };

Pixel pixels[LED_COUNT] = {};
Pixel previousPixels[LED_COUNT] = {};
uint32_t lastUpdate = 0;
uint32_t lastErrorUpdate = 0;
bool hasPreviousFrame = false;

#define NS_PER_SEC 1000000000L
#define NS_PER_CYCLE (NS_PER_SEC / F_CPU)
#define NS_TO_CYCLES(value) ((value) / NS_PER_CYCLE)

inline void sendBit(bool value)
{
  if (value) {
    asm volatile(
      "sbi %[port], %[bit] \n\t"
      ".rept %[onCycles] \n\t"
      "nop \n\t"
      ".endr \n\t"
      "cbi %[port], %[bit] \n\t"
      ".rept %[offCycles] \n\t"
      "nop \n\t"
      ".endr \n\t"
      :: [port] "I" (_SFR_IO_ADDR(PORTD)), [bit] "I" (PD5),
         [onCycles] "I" (NS_TO_CYCLES(800) - 2),
         [offCycles] "I" (NS_TO_CYCLES(400) - 2));
  } else {
    asm volatile(
      "sbi %[port], %[bit] \n\t"
      ".rept %[onCycles] \n\t"
      "nop \n\t"
      ".endr \n\t"
      "cbi %[port], %[bit] \n\t"
      ".rept %[offCycles] \n\t"
      "nop \n\t"
      ".endr \n\t"
      :: [port] "I" (_SFR_IO_ADDR(PORTD)), [bit] "I" (PD5),
         [onCycles] "I" (NS_TO_CYCLES(400) - 2),
         [offCycles] "I" (NS_TO_CYCLES(800) - 2));
  }
}

inline void sendByte(uint8_t value)
{
  for (uint8_t bit = 0; bit < 8; ++bit) {
    sendBit(value & 0x80);
    value <<= 1;
  }
}

void showPixels()
{
  // Once the first WS2812 bit is emitted, preserve the entire waveform. Keep
  // Timer1's compare flag pending while interrupts are masked; its servo ISR
  // runs immediately after the frame and resumes the scheduled servo edges.
  uint8_t oldSreg = SREG;
  DmxInput::pauseReception();
  cli();
  for (uint8_t i = 0; i < LED_COUNT; ++i) {
    sendByte(pixels[i].green);
    sendByte(pixels[i].red);
    sendByte(pixels[i].blue);
  }
  SREG = oldSreg;
  _delay_us(6);
  DmxInput::resumeReception();
}

bool writeIfChanged()
{
  bool changed = !hasPreviousFrame;
  for (uint8_t i = 0; i < LED_COUNT; ++i) {
    if (pixels[i].red != previousPixels[i].red ||
        pixels[i].green != previousPixels[i].green ||
        pixels[i].blue != previousPixels[i].blue) {
      changed = true;
    }
  }
  if (!changed) return true;
  if (LedEngine::canTransmitNow()) {
    showPixels();
    for (uint8_t i = 0; i < LED_COUNT; ++i) previousPixels[i] = pixels[i];
    hasPreviousFrame = true;
    return true;
  }
  return false;
}

uint8_t triangle(uint8_t phase)
{
  return phase < 128 ? static_cast<uint8_t>(phase << 1)
                     : static_cast<uint8_t>(255 - ((phase - 128) << 1));
}

uint8_t scale(uint8_t value, uint8_t amount)
{
  return static_cast<uint8_t>((static_cast<uint16_t>(value) * amount) / 255U);
}

}

namespace LedEngine {

bool canTransmitNow()
{
  return !AudioEngine::busy() && ServoEngine::canBlockFor(300);
}

void begin()
{
  DDRD |= _BV(PD5);
  PORTD &= static_cast<uint8_t>(~_BV(PD5));
  clear();
}

void update(const Frame &frame, uint32_t now)
{
  if (now - lastUpdate >= LED_UPDATE_INTERVAL_MS) {
    lastUpdate = now;

    uint8_t red = scale(frame.red, frame.dimmer);
    uint8_t green = scale(frame.green, frame.dimmer);
    uint8_t blue = scale(frame.blue, frame.dimmer);
    uint8_t phase = static_cast<uint8_t>((now * (1UL + frame.rate / 32U)) >> 4);

    for (uint8_t i = 0; i < LED_COUNT; ++i) {
      uint8_t r = red;
      uint8_t g = green;
      uint8_t b = blue;

      if (frame.effect < 16) {
        // Static RGB.
      } else if (frame.effect < 48) {
        uint8_t level = triangle(phase);
        r = scale(r, level); g = scale(g, level); b = scale(b, level);
      } else if (frame.effect < 80) {
        uint8_t position = static_cast<uint8_t>((phase >> 4) % LED_COUNT);
        if (i != position) { r >>= 2; g >>= 2; b >>= 2; }
      }

      pixels[i].red = r;
      pixels[i].green = g;
      pixels[i].blue = b;
    }
  }
  // Retry a changed frame on every scheduler pass. The update interval controls
  // animation rendering, not whether a pending WS2812 write gets another chance.
  writeIfChanged();
}

void clear()
{
  for (uint8_t i = 0; i < LED_COUNT; ++i) pixels[i] = {0, 0, 0};
  writeIfChanged();
}

void showAddressError(uint32_t now)
{
  if (now - lastErrorUpdate < 300) return;
  lastErrorUpdate = now;
  uint8_t level = ((now / 300U) & 1U) ? 32 : 0;
  for (uint8_t i = 0; i < LED_COUNT; ++i) pixels[i] = {level, 0, 0};
  writeIfChanged();
}

}
