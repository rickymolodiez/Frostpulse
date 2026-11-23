#pragma once
#include <Arduino.h>

namespace Haptic {
  void begin(uint8_t pin);
  void update();
  void pulse();
  void buzz();
  void stop();
  bool isActive();
}