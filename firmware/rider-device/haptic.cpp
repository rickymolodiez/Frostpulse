// haptic.cpp
#include "haptic.h"

namespace {
  struct Step {
    uint16_t ms;
    bool on;
  };
}

static uint8_t motorPin;
static const Step* steps = nullptr;
static uint8_t stepCount = 0;
static uint8_t stepIndex = 0;
static unsigned long stepStart = 0;

static const Step PULSE_PATTERN[] = {
  {150, true}
};

static const Step BUZZ_PATTERN[] = {
  {500, true}, {200, false},
  {500, true}, {200, false},
  {500, true}
};

static void applyStep() {
  digitalWrite(motorPin, steps[stepIndex].on ? HIGH : LOW);
  stepStart = millis();
}

static void play(const Step* pattern, uint8_t count) {
  steps = pattern;
  stepCount = count;
  stepIndex = 0;
  applyStep();
}

namespace Haptic {
  void begin(uint8_t pin) {
    motorPin = pin;
    pinMode(motorPin, OUTPUT);
    digitalWrite(motorPin, LOW);
  }

  void update() {
    if (!steps) return;

    if (millis() - stepStart >= steps[stepIndex].ms) {
      stepIndex++;
      if (stepIndex >= stepCount) {
        stop();
      } else {
        applyStep();
      }
    }
  }

  void pulse() { play(PULSE_PATTERN, sizeof(PULSE_PATTERN) / sizeof(Step)); }
  void buzz()  { play(BUZZ_PATTERN,  sizeof(BUZZ_PATTERN)  / sizeof(Step)); }

  void stop() {
    steps = nullptr;
    digitalWrite(motorPin, LOW);
  }

  bool isActive() { return steps != nullptr; }
}