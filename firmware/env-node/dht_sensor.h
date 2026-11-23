// DHT11 temperature/humidity reads with validation and stale detection.
#pragma once
#include <Arduino.h>

enum class SensorStatus : uint8_t {
  NO_DATA,  // no good reading since boot
  OK,       // latest good reading is fresh
  STALE,    // last good reading is older than the stale limit
};

struct Reading {
  float tempC = NAN;
  float humidity = NAN;
  float dewPointC = NAN;
  uint32_t takenAtMs = 0;  // millis() of the reading
};

namespace dht_sensor {

void begin();

// Read interval and stale limit; can change at runtime (remote config).
void setTiming(uint32_t readIntervalMs, uint32_t staleAfterMs);

// Call every loop(). Reads the sensor once the read interval has passed.
// Returns true when a new good reading was stored on this call.
bool update(uint32_t nowMs);

// Last good reading (values are NAN until the first good read).
const Reading& latest();

SensorStatus status(uint32_t nowMs);
uint32_t ageMs(uint32_t nowMs);          // age of latest(); 0 if NO_DATA
uint16_t consecutiveFailures();          // failed/rejected reads in a row
bool inRatedRange(const Reading& r);     // within the DHT11 datasheet range
const char* statusName(SensorStatus s);  // "ok", "stale", "no_data"

}  // namespace dht_sensor
