#include "dht_sensor.h"

#include <DHT.h>
#include <math.h>

#include "config.h"

namespace {

DHT dht(DHT_PIN, DHT_TYPE);
Reading last;
bool haveReading = false;
bool attempted = false;
uint32_t lastAttemptMs = 0;
uint16_t failures = 0;
uint32_t readIntervalMs = DEFAULT_READ_INTERVAL_MS;
uint32_t staleAfterMs = DEFAULT_STALE_AFTER_MS;

bool plausible(float t, float h) {
  return !isnan(t) && !isnan(h) && t >= TEMP_MIN_C && t <= TEMP_MAX_C &&
         h >= 0.0f && h <= 100.0f;
}

// Magnus formula, good to ~0.35 C over -45..60 C.
float dewPoint(float t, float rh) {
  if (rh <= 0.0f) return NAN;
  const float a = 17.62f, b = 243.12f;
  const float g = logf(rh / 100.0f) + a * t / (b + t);
  return b * g / (a - g);
}

void recordFailure(const char* why, float t, float h) {
  if (failures < UINT16_MAX) failures++;
  Serial.printf("[dht] rejected read (%s): t=%.1f h=%.1f, %u in a row\n", why,
                t, h, failures);
}

}  // namespace

namespace dht_sensor {

void begin() { dht.begin(); }

void setTiming(uint32_t readMs, uint32_t staleMs) {
  readIntervalMs = readMs;
  staleAfterMs = staleMs;
}

bool update(uint32_t nowMs) {
  if (attempted && nowMs - lastAttemptMs < readIntervalMs) return false;
  attempted = true;
  lastAttemptMs = nowMs;

  const float h = dht.readHumidity();
  const float t = dht.readTemperature();  // Celsius

  if (!plausible(t, h)) {
    recordFailure(isnan(t) || isnan(h) ? "no response" : "out of range", t, h);
    return false;
  }
  // Single-sample spike filter. Skipped once the last value is stale so a
  // real change is accepted eventually instead of being rejected forever.
  if (status(nowMs) == SensorStatus::OK &&
      fabsf(t - last.tempC) > MAX_TEMP_JUMP_C) {
    recordFailure("spike", t, h);
    return false;
  }

  failures = 0;
  last.tempC = t;
  last.humidity = h;
  last.dewPointC = dewPoint(t, h);
  last.takenAtMs = nowMs;
  haveReading = true;
  return true;
}

const Reading& latest() { return last; }

SensorStatus status(uint32_t nowMs) {
  if (!haveReading) return SensorStatus::NO_DATA;
  return (nowMs - last.takenAtMs > staleAfterMs) ? SensorStatus::STALE
                                                 : SensorStatus::OK;
}

uint32_t ageMs(uint32_t nowMs) {
  return haveReading ? nowMs - last.takenAtMs : 0;
}

uint16_t consecutiveFailures() { return failures; }

bool inRatedRange(const Reading& r) {
  return r.tempC >= RATED_TEMP_MIN_C && r.tempC <= RATED_TEMP_MAX_C &&
         r.humidity >= RATED_RH_MIN && r.humidity <= RATED_RH_MAX;
}

const char* statusName(SensorStatus s) {
  switch (s) {
    case SensorStatus::OK:    return "ok";
    case SensorStatus::STALE: return "stale";
    default:                  return "no_data";
  }
}

}  // namespace dht_sensor
