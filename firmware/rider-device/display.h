// display.h
#pragma once
#include <Arduino.h>

namespace Display {
  bool begin(uint8_t sda, uint8_t scl, int8_t rstPin = -1);
  void update();

  void showStatus(const char* msg, uint16_t durationMs = 3000);  // temporary
  void showWarning(const char* msg); 
  void clearWarning();
  bool warningActive();

  void setWifiConnected(bool connected);
}