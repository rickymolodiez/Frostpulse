// FrostPulse env-node (TTGO ESP32 with built-in OLED + DHT11)
//
// - Reads air temperature/humidity, rejects bad reads and flags stale data.
// - Shows live conditions and device status on the OLED.
// - Queues one timestamped observation per upload interval and POSTs them to
//   the cloud; while the network is down they wait in RAM (QUEUE_SIZE).
// - Polls the cloud for settings (read/upload interval, stale limit) and
//   saves accepted ones to flash.
//
// The readings give context next to road reports; they do NOT detect ice.
//
// Setup:
//   1. Copy config.example.h to config.h and fill in Wi-Fi + API values.
//   2. Board: "ESP32 Dev Module" or "TTGO LoRa32-OLED" (Arduino-ESP32 core).
//   3. Libraries: "DHT sensor library" + "Adafruit Unified Sensor",
//      "Adafruit SSD1306" + "Adafruit GFX Library", "ArduinoJson" (v7).

#if __has_include("config.h")
#include "config.h"
#else
#error "config.h missing: copy config.example.h to config.h and edit it"
#endif

#include <esp_random.h>
#include <stdarg.h>

#include "dht_sensor.h"
#include "display.h"
#include "net.h"
#include "settings.h"

// One queued upload. Values are captured when the observation is taken, not
// when it is sent, so readings stay correct after a network outage.
struct Observation {
  uint32_t seq;
  uint32_t takenAtMs;      // millis() when captured
  time_t epoch;            // UTC seconds when captured (0 if NTP not synced)
  SensorStatus status;
  float tempC, humidity, dewPointC;
  uint32_t sensorAgeMs;    // age of the sensor reading at capture time
  uint16_t sensorFailures;
  bool inRatedRange;
};

static Observation obsQueue[QUEUE_SIZE];
static uint16_t qHead = 0, qCount = 0;
static uint32_t dropped = 0;   // observations lost because the queue was full

static char bootId[9];         // random per boot, so (boot_id, seq) is unique
static uint32_t nextSeq = 1;
static uint32_t nextCaptureMs = 0;
static uint32_t nextSendMs = 0;
static uint32_t nextSettingsPollMs = 0;
static uint32_t lastDrawMs = 0;
static int lastHttpCode = 0;

// ---------------------------------------------------------------- queue ----

static void enqueue(const Observation& o) {
  if (qCount == QUEUE_SIZE) {  // full: drop the oldest
    qHead = (qHead + 1) % QUEUE_SIZE;
    qCount--;
    dropped++;
  }
  obsQueue[(qHead + qCount) % QUEUE_SIZE] = o;
  qCount++;
}

static void dequeue() {
  qHead = (qHead + 1) % QUEUE_SIZE;
  qCount--;
}

static void captureObservation(uint32_t nowMs) {
  const Reading& r = dht_sensor::latest();
  Observation o;
  o.seq = nextSeq++;
  o.takenAtMs = nowMs;
  o.epoch = net::epochNow();
  o.status = dht_sensor::status(nowMs);
  o.tempC = r.tempC;
  o.humidity = r.humidity;
  o.dewPointC = r.dewPointC;
  o.sensorAgeMs = dht_sensor::ageMs(nowMs);
  o.sensorFailures = dht_sensor::consecutiveFailures();
  o.inRatedRange = dht_sensor::inRatedRange(r);
  enqueue(o);
  Serial.printf("[main] captured seq=%lu status=%s queued=%u\n",
                (unsigned long)o.seq, dht_sensor::statusName(o.status), qCount);
}

// -------------------------------------------------------------- payload ----

// snprintf that appends at `pos`; returns the new position.
static size_t appendf(char* buf, size_t len, size_t pos, const char* fmt, ...) {
  if (pos >= len) return pos;
  va_list ap;
  va_start(ap, fmt);
  const int n = vsnprintf(buf + pos, len - pos, fmt, ap);
  va_end(ap);
  return n < 0 ? pos : pos + n;
}

// JSON body for POST READINGS_PATH (see docs/api.md). Measurement values are
// null unless status == "ok", so an old value is never taken as current.
static bool buildPayload(const Observation& o, char* buf, size_t len,
                         uint32_t nowMs) {
  const uint32_t ageMs = nowMs - o.takenAtMs;
  // If NTP synced only after capture, derive the capture time now.
  time_t epoch = o.epoch;
  if (epoch == 0 && net::epochNow() != 0) {
    epoch = net::epochNow() - (time_t)(ageMs / 1000);
  }

  size_t p = 0;
  p = appendf(buf, len, p, "{\"node_id\":\"%s\",\"boot_id\":\"%s\",\"seq\":%lu",
              NODE_ID, bootId, (unsigned long)o.seq);
  if (epoch != 0) {
    char ts[24];
    net::formatIso(epoch, ts, sizeof ts);
    p = appendf(buf, len, p, ",\"observed_at\":\"%s\"", ts);
  } else {
    p = appendf(buf, len, p, ",\"observed_at\":null");
  }
  p = appendf(buf, len, p, ",\"age_ms\":%lu,\"status\":\"%s\"",
              (unsigned long)ageMs, dht_sensor::statusName(o.status));
  if (o.status == SensorStatus::OK) {
    p = appendf(buf, len, p,
                ",\"temp_c\":%.1f,\"humidity\":%.1f,\"dew_point_c\":%.1f,"
                "\"in_rated_range\":%s",
                o.tempC, o.humidity, o.dewPointC,
                o.inRatedRange ? "true" : "false");
  } else {
    p = appendf(buf, len, p,
                ",\"temp_c\":null,\"humidity\":null,\"dew_point_c\":null,"
                "\"in_rated_range\":null");
  }
  if (o.status == SensorStatus::NO_DATA) {
    p = appendf(buf, len, p, ",\"sensor_age_ms\":null");
  } else {
    p = appendf(buf, len, p, ",\"sensor_age_ms\":%lu",
                (unsigned long)o.sensorAgeMs);
  }
  p = appendf(buf, len, p,
              ",\"sensor_failures\":%u,\"lat\":%.6f,\"lon\":%.6f,\"rssi\":%d,"
              "\"uptime_ms\":%lu,\"config_version\":%lu,\"queue_depth\":%u,"
              "\"dropped\":%lu",
              o.sensorFailures, (double)NODE_LAT, (double)NODE_LON,
              net::rssi(), (unsigned long)nowMs,
              (unsigned long)settings::get().version, qCount,
              (unsigned long)dropped);
  const char* cfgErr = settings::lastError();
  if (cfgErr[0]) {
    p = appendf(buf, len, p, ",\"config_error\":\"%s\"}", cfgErr);
  } else {
    p = appendf(buf, len, p, ",\"config_error\":null}");
  }
  return p < len;  // false if truncated
}

// ------------------------------------------------------------ uploading ----

static void sendOldest(uint32_t nowMs) {
  char body[640];
  const Observation& o = obsQueue[qHead];
  if (!buildPayload(o, body, sizeof body, nowMs)) {
    Serial.println("[main] payload too large, dropping observation");
    dequeue();
    return;
  }
  Serial.printf("[main] POST %s\n", body);
  lastHttpCode = net::postJson(READINGS_PATH, body);
  Serial.printf("[main] -> %d\n", lastHttpCode);

  const int c = lastHttpCode;
  if (c >= 200 && c < 300) {
    dequeue();
    nextSendMs = millis();  // drain the rest of the queue right away
  } else if (c >= 400 && c < 500 && c != 401 && c != 408 && c != 429) {
    // The server says this payload is invalid; retrying won't help and would
    // block everything behind it.
    Serial.println("[main] server rejected observation, dropping it");
    dequeue();
    nextSendMs = millis();
  } else {
    // Network error, 5xx, auth or rate limit: keep it and retry later.
    nextSendMs = millis() + POST_RETRY_MS;
  }
}

// ------------------------------------------------------- remote config ----

static void applyTiming() {
  const Settings& s = settings::get();
  dht_sensor::setTiming(s.readIntervalMs, s.staleAfterMs);
}

static void pollSettings() {
  String body;
  const int code = net::getJson(SETTINGS_PATH, body);
  if (code == 404) return;  // no settings defined for this node: keep current
  if (code != 200) {
    Serial.printf("[cfg] settings fetch failed: %d\n", code);
    return;
  }
  switch (settings::applyJson(body.c_str(), body.length())) {
    case ApplyResult::APPLIED: {
      const Settings& s = settings::get();
      applyTiming();
      // Start the new upload interval from now.
      nextCaptureMs = millis() + s.uploadIntervalMs;
      Serial.printf("[cfg] applied v%lu read=%lums upload=%lums stale=%lums\n",
                    (unsigned long)s.version, (unsigned long)s.readIntervalMs,
                    (unsigned long)s.uploadIntervalMs,
                    (unsigned long)s.staleAfterMs);
      break;
    }
    case ApplyResult::REJECTED:
      Serial.printf("[cfg] rejected settings: %s\n", settings::lastError());
      break;
    case ApplyResult::UNCHANGED:
      break;
  }
}

// ------------------------------------------------------------ main loop ----

void setup() {
  Serial.begin(115200);
  delay(200);
  snprintf(bootId, sizeof bootId, "%08lx", (unsigned long)esp_random());
  Serial.printf("\nFrostPulse env-node %s boot %s\n", NODE_ID, bootId);

  display::begin();
  display::message("FrostPulse env-node", NODE_ID);
  settings::begin();
  applyTiming();
  dht_sensor::begin();
  net::begin();
}

void loop() {
  const uint32_t now = millis();

  net::update(now);

  if (dht_sensor::update(now)) {
    const Reading& r = dht_sensor::latest();
    Serial.printf("[dht] t=%.1fC h=%.1f%% dp=%.1fC\n", r.tempC, r.humidity,
                  r.dewPointC);
  }

  // Capture observations on schedule, online or not. Right after boot, give
  // the sensor until the stale limit to produce a first reading.
  const bool bootGrace = dht_sensor::status(now) == SensorStatus::NO_DATA &&
                         now < settings::get().staleAfterMs;
  if (!bootGrace && (int32_t)(now - nextCaptureMs) >= 0) {
    captureObservation(now);
    nextCaptureMs = now + settings::get().uploadIntervalMs;
  }

  if (net::connected()) {
    if ((int32_t)(now - nextSettingsPollMs) >= 0) {
      pollSettings();
      nextSettingsPollMs = millis() + SETTINGS_POLL_MS;
    }
    if (qCount > 0 && (int32_t)(now - nextSendMs) >= 0) {
      sendOldest(millis());
    }
  }

  if (now - lastDrawMs >= DISPLAY_REFRESH_MS) {
    lastDrawMs = now;
    NodeStatus node{net::connected(), lastHttpCode, qCount,
                    settings::get().version, settings::lastError()[0] != '\0'};
    display::render(dht_sensor::latest(), dht_sensor::status(now),
                    dht_sensor::ageMs(now), node);
  }
}
