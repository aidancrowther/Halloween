#pragma once

#include <Arduino.h>

// Hardware pin assignments. Arduino pin numbers match the ATmega8 standard variant.
constexpr uint8_t SERVO_LEFT_PIN = 2;       // PD2
constexpr uint8_t SERVO_RIGHT_PIN = 3;      // PD3
constexpr uint8_t SERVO_BEAK_PIN = 4;       // PD4
constexpr uint8_t LED_DATA_PIN = 5;         // PD5
constexpr uint8_t DFPLAYER_TX_PIN = 6;      // PD6
constexpr uint8_t DMX_MODE_PIN = 7;         // PD7, dummy receive-only mode pin

constexpr uint8_t LED_COUNT = 7;
constexpr uint8_t DMX_FOOTPRINT = 13;
constexpr uint32_t DFPLAYER_BAUD = 9600UL;

// Servo calibration. Save operating limits as angles; the firmware converts them
// to pulse widths. Calibrate these values against the installed mechanism before
// final deployment.
constexpr uint8_t LEFT_WING_MIN_ANGLE = 8;   // Minimum safe operating angle
constexpr uint8_t LEFT_WING_MAX_ANGLE = 172; // Maximum safe operating angle
constexpr uint8_t RIGHT_WING_MIN_ANGLE = 8;  // Minimum safe operating angle
constexpr uint8_t RIGHT_WING_MAX_ANGLE = 172;// Maximum safe operating angle
constexpr uint8_t BEAK_MIN_ANGLE = 113;      // Minimum: beak closed/parked
constexpr uint8_t BEAK_MAX_ANGLE = 180;      // Maximum: beak open

// Static-mode DMX input calibration for the beak.
constexpr uint8_t BEAK_CLOSED_INPUT = 128;
constexpr uint8_t BEAK_OPEN_INPUT = 255;

// Safe positions used when DMX is lost.
constexpr uint8_t LEFT_WING_PARK_ANGLE = 172;
constexpr uint8_t RIGHT_WING_PARK_ANGLE = 172;
constexpr uint8_t BEAK_PARK_ANGLE = 113;

constexpr uint16_t SERVO_MIN_US = 544;
constexpr uint16_t SERVO_MAX_US = 2400;

constexpr uint16_t angleToMicroseconds(uint8_t angle)
{
  return SERVO_MIN_US +
         (static_cast<uint32_t>(SERVO_MAX_US - SERVO_MIN_US) * angle) / 180UL;
}

constexpr uint16_t LEFT_WING_MIN_US = angleToMicroseconds(LEFT_WING_MIN_ANGLE);
constexpr uint16_t LEFT_WING_MAX_US = angleToMicroseconds(LEFT_WING_MAX_ANGLE);
constexpr uint16_t RIGHT_WING_MIN_US = angleToMicroseconds(RIGHT_WING_MIN_ANGLE);
constexpr uint16_t RIGHT_WING_MAX_US = angleToMicroseconds(RIGHT_WING_MAX_ANGLE);
constexpr uint16_t BEAK_MIN_US = angleToMicroseconds(BEAK_MIN_ANGLE);
constexpr uint16_t BEAK_MAX_US = angleToMicroseconds(BEAK_MAX_ANGLE);
constexpr uint16_t LEFT_WING_PARK_US = angleToMicroseconds(LEFT_WING_PARK_ANGLE);
constexpr uint16_t RIGHT_WING_PARK_US = angleToMicroseconds(RIGHT_WING_PARK_ANGLE);
constexpr uint16_t BEAK_PARK_US = angleToMicroseconds(BEAK_PARK_ANGLE);

constexpr uint16_t SERVO_FRAME_US = 20000;
constexpr uint16_t SERVO_REFRESH_US = 20000;
constexpr uint16_t SERVO_UPDATE_INTERVAL_MS = 20;
constexpr uint16_t LED_UPDATE_INTERVAL_MS = 40;
constexpr uint32_t DMX_TIMEOUT_MS = 1000UL;
// Delay between repeated preset cycles; this is independent of packet/motion
// sequencing within each cycle.
constexpr uint32_t PRESET_COOLDOWN_MS = 10000UL;
constexpr uint16_t SERVO_SLEW_MIN_US = 4;
constexpr uint16_t SERVO_SLEW_MAX_US = 48;

// DMX personality channel offsets.
enum DmxChannel : uint8_t {
  CHANNEL_MODE = 0,
  CHANNEL_SPEED = 1,
  CHANNEL_LEFT_WING = 2,
  CHANNEL_RIGHT_WING = 3,
  CHANNEL_BEAK = 4,
  CHANNEL_DIMMER = 5,
  CHANNEL_RED = 6,
  CHANNEL_GREEN = 7,
  CHANNEL_BLUE = 8,
  CHANNEL_LED_EFFECT = 9,
  CHANNEL_EFFECT_RATE = 10,
  CHANNEL_SOUND = 11,
  CHANNEL_VOLUME = 12
};

// DFPlayer track assignments for the current SD card layout.
// Files are numbered at the card root as /0001.mp3 through /0010.mp3.
constexpr uint16_t TRACK_RANDOM_BIRD = 1; // 0001.mp3: primary bird call
constexpr uint16_t TRACK_LIGHTNING = 2;   // 0002.mp3: ominous/deep call
constexpr uint16_t TRACK_WING_FLAP = 5;   // 0005.mp3: heavy wing flap
constexpr uint16_t TRACK_CAGE_RATTLE = 9; // 0009.mp3: cage/bone rattle
constexpr uint16_t TRACK_CAW = 1;         // 0001.mp3: primary crow/raven call
constexpr uint16_t TRACK_SCREECH = 4;     // 0004.mp3: short screech
