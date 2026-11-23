// Built-in OLED (SSD1306) status screen for the env-node.
#pragma once
#include <Arduino.h>

#include "dht_sensor.h"

struct NodeStatus {
  bool wifiUp;
  int lastHttpCode;        // 0 = nothing sent yet, >0 HTTP status, <0 error
  uint16_t queued;         // observations waiting to upload
  uint32_t configVersion;  // 0 = built-in defaults
  bool configError;        // last settings update was rejected
};

namespace display {

// Returns false if the OLED wasn't found; all other calls are then no-ops,
// so the node keeps measuring and uploading without a screen.
bool begin();

// Two-line message, e.g. during boot.
void message(const char* line1, const char* line2 = "");

void render(const Reading& r, SensorStatus s, uint32_t readingAgeMs,
            const NodeStatus& node);

}  // namespace display
