#include <Arduino.h>
#include <DMXSerial.h>

// DMXSerial requires a direction-control pin. The MAX485 on this board is
// permanently wired for receive, so this is intentionally unused. PD1/TXD is
// left available for future diagnostics.
static const uint8_t DMX_DIRECTION_PIN = 2; // PD2 / Arduino D2

// Relay outputs are active low: PB0, PD5, PD6, PD7.
static const uint8_t RELAY_PINS[4] = {8, 5, 6, 7};

// Address inputs use the ATmega8 internal pull-ups and are active low. The
// physical switch order is reversed relative to the logical address bits:
// PB1..PB5, PC0..PC2 represent bits 8..4, 3..1 respectively.
static const uint8_t ADDRESS_B_MASK = _BV(PB1) | _BV(PB2) | _BV(PB3) |
                                      _BV(PB4) | _BV(PB5);
static const uint8_t ADDRESS_C_MASK = _BV(PC0) | _BV(PC1) | _BV(PC2);

static const uint16_t DMX_CHANNEL_COUNT = 7;
static const uint16_t MAX_VALID_START_ADDRESS = 512 - DMX_CHANNEL_COUNT + 1;
static const unsigned long DMX_TIMEOUT_MS = 1000UL;
static const uint16_t RELAY_MIN_TRANSITION_MS = 100;

enum MachineMode : uint8_t {
  MODE_OFF,
  MODE_IDLE,
  MODE_BURST,
  MODE_CYCLE,
  MODE_CONTINUOUS
};

enum TimerType : uint8_t {
  TIMER_NONE,
  TIMER_BURST,
  TIMER_CYCLE_ON,
  TIMER_CYCLE_OFF
};

struct MachineState {
  bool automaticArmed;
  TimerType timer;
  unsigned long deadline;
};

static MachineState machines[4];
static uint16_t startAddress = 0;
static uint8_t relayState = 0;
static unsigned long lastRelayChange[4] = {0, 0, 0, 0};
static bool lastMasterEnabled = false;

static bool deadlineReached(unsigned long now, unsigned long deadline) {
  return (long)(now - deadline) >= 0;
}

static uint32_t mapDuration(uint8_t value, uint32_t minimum, uint32_t maximum) {
  return minimum + ((uint32_t)value * (maximum - minimum)) / 255UL;
}

static uint32_t burstDurationMs(uint8_t value) {
  return mapDuration(value, 250UL, 30000UL);
}

static uint32_t cycleOffDurationMs(uint8_t value) {
  return mapDuration(value, 2000UL, 300000UL);
}

static uint16_t readStartAddress() {
  // Configure the pins explicitly before sampling. This avoids any transient
  // output state during startup and matches the board's physical wiring.
  DDRB &= (uint8_t)~ADDRESS_B_MASK;
  DDRC &= (uint8_t)~ADDRESS_C_MASK;
  PORTB |= ADDRESS_B_MASK;
  PORTC |= ADDRESS_C_MASK;
  delay(10);

  // Sample several times so switch bounce or a slowly settling input cannot
  // produce a different address at startup.
  uint8_t b1 = 0, b2 = 0, b3 = 0, b4 = 0, b5 = 0;
  uint8_t c0 = 0, c1 = 0, c2 = 0;
  for (uint8_t sample = 0; sample < 9; ++sample) {
    const uint8_t selectedB = (uint8_t)~PINB;
    const uint8_t selectedC = (uint8_t)~PINC;
    if (selectedB & _BV(PB1)) ++b1;
    if (selectedB & _BV(PB2)) ++b2;
    if (selectedB & _BV(PB3)) ++b3;
    if (selectedB & _BV(PB4)) ++b4;
    if (selectedB & _BV(PB5)) ++b5;
    if (selectedC & _BV(PC0)) ++c0;
    if (selectedC & _BV(PC1)) ++c1;
    if (selectedC & _BV(PC2)) ++c2;
    delay(2);
  }

  uint16_t address = 1; // Address bit 1 is permanently set by design.
  if (b1 >= 5) address |= _BV(8);
  if (b2 >= 5) address |= _BV(7);
  if (b3 >= 5) address |= _BV(6);
  if (b4 >= 5) address |= _BV(5);
  if (b5 >= 5) address |= _BV(4);
  if (c0 >= 5) address |= _BV(3);
  if (c1 >= 5) address |= _BV(2);
  if (c2 >= 5) address |= _BV(1);
  return address;
}

static MachineMode decodeMode(uint8_t command) {
  if (command <= 9) return MODE_OFF;
  if (command <= 99) return MODE_IDLE;
  if (command <= 149) return MODE_BURST;
  if (command <= 199) return MODE_CYCLE;
  return MODE_CONTINUOUS;
}

static void cancelMachine(uint8_t index, bool armAutomatic) {
  machines[index].timer = TIMER_NONE;
  machines[index].deadline = 0;
  machines[index].automaticArmed = armAutomatic;
}

static void disarmAllMachines() {
  for (uint8_t i = 0; i < 4; ++i) {
    cancelMachine(i, false);
  }
}

static bool updateMachine(uint8_t index, uint8_t command, uint8_t onValue,
                          uint8_t offValue, unsigned long now) {
  MachineState &machine = machines[index];
  const MachineMode mode = decodeMode(command);

  // Explicit off cancels activity and arms the automatic modes.
  if (mode == MODE_OFF) {
    cancelMachine(index, true);
    return false;
  }

  // A cycle is active only while its command remains in the cycle range.
  if (machine.timer == TIMER_CYCLE_ON || machine.timer == TIMER_CYCLE_OFF) {
    if (mode != MODE_CYCLE) {
      cancelMachine(index, false);
    } else if (deadlineReached(now, machine.deadline)) {
      if (machine.timer == TIMER_CYCLE_ON) {
        machine.timer = TIMER_CYCLE_OFF;
        machine.deadline = now + cycleOffDurationMs(offValue);
      } else {
        machine.timer = TIMER_CYCLE_ON;
        machine.deadline = now + burstDurationMs(onValue);
      }
    }
  }

  // A one-shot completes even if the command falls into the idle range.
  if (machine.timer == TIMER_BURST && deadlineReached(now, machine.deadline)) {
    machine.timer = TIMER_NONE;
  }

  // A burst is edge-triggered and requires a prior visit to the explicit off
  // range. A cycle is level-triggered: while its command remains in the cycle
  // range it starts/restarts independently for this machine. This allows a
  // cycle on one relay to coexist with burst, idle, or continuous modes on the
  // other relays, and permits a cycle command held high at startup to work.
  if (machine.timer == TIMER_NONE) {
    if (mode == MODE_BURST && machine.automaticArmed) {
      machine.automaticArmed = false;
      machine.timer = TIMER_BURST;
      machine.deadline = now + burstDurationMs(onValue);
    } else if (mode == MODE_CYCLE) {
      machine.timer = TIMER_CYCLE_ON;
      machine.deadline = now + burstDurationMs(onValue);
    }
  }

  if (mode == MODE_CONTINUOUS) return true;
  return machine.timer == TIMER_BURST || machine.timer == TIMER_CYCLE_ON;
}

static void applyRelayOutputs(uint8_t desiredState, unsigned long now,
                              bool immediateOff) {
  for (uint8_t i = 0; i < 4; ++i) {
    const uint8_t mask = (uint8_t)(1U << i);
    const bool desired = (desiredState & mask) != 0;
    const bool current = (relayState & mask) != 0;

    if (desired == current) continue;
    if (!immediateOff && (uint16_t)(now - lastRelayChange[i]) < RELAY_MIN_TRANSITION_MS) {
      continue;
    }

    // The logical state is active high, but the physical relay input is
    // active low.
    digitalWrite(RELAY_PINS[i], desired ? LOW : HIGH);
    lastRelayChange[i] = now;
    if (desired) relayState |= mask;
    else relayState &= (uint8_t)~mask;
  }
}

static void forceAllRelaysOff() {
  for (uint8_t i = 0; i < 4; ++i) {
    digitalWrite(RELAY_PINS[i], HIGH);
  }
  relayState = 0;
}

static void setupPins() {
  // Drive active-low relay outputs high before enabling their output drivers.
  PORTB |= _BV(PB0);
  PORTD |= _BV(PD5) | _BV(PD6) | _BV(PD7);
  DDRB |= _BV(DDB0);
  DDRD |= _BV(DDD5) | _BV(DDD6) | _BV(DDD7);

  for (uint8_t i = 0; i < 4; ++i) {
    pinMode(RELAY_PINS[i], OUTPUT);
    digitalWrite(RELAY_PINS[i], HIGH);
  }

  // Address pins were configured directly in readStartAddress().
}

void setup() {
  setupPins();
  delay(5); // Allow address inputs to settle before sampling.

  startAddress = readStartAddress();
  forceAllRelaysOff();
  disarmAllMachines();

  if (startAddress > MAX_VALID_START_ADDRESS) return;

  // PD2 is a harmless dummy output for DMXSerial. The MAX485 direction is
  // hard-wired to receive. PD1/TXD remains untouched for future diagnostics.
  DMXSerial.init(DMXReceiver, DMX_DIRECTION_PIN);
  DMXSerial.maxChannel(startAddress + DMX_CHANNEL_COUNT - 1);
}

void loop() {
  const unsigned long now = millis();

  if (startAddress == 0 || startAddress > MAX_VALID_START_ADDRESS) {
    forceAllRelaysOff();
    return;
  }

  if (DMXSerial.noDataSince() > DMX_TIMEOUT_MS) {
    disarmAllMachines();
    forceAllRelaysOff();
    lastMasterEnabled = false;
    return;
  }

  const bool masterEnabled = DMXSerial.read(startAddress + 6) >= 128;

  if (!masterEnabled) {
    disarmAllMachines();
    applyRelayOutputs(0, now, true);
    lastMasterEnabled = false;
    return;
  }

  // Enabling the master never fires an automatic command already held high.
  if (!lastMasterEnabled) disarmAllMachines();
  lastMasterEnabled = true;

  uint8_t desiredState = 0;
  const uint8_t onValue = DMXSerial.read(startAddress + 4);
  const uint8_t offValue = DMXSerial.read(startAddress + 5);

  for (uint8_t i = 0; i < 4; ++i) {
    const uint8_t command = DMXSerial.read(startAddress + i);
    if (updateMachine(i, command, onValue, offValue, now)) {
      desiredState |= (uint8_t)(1U << i);
    }
  }

  applyRelayOutputs(desiredState, now, false);
}