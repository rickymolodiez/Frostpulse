#include "settings.h"

#include <ArduinoJson.h>
#include <Preferences.h>

#include "config.h"

namespace {

Settings current;
char error[64] = "";
const char* NVS_NAMESPACE = "envcfg";

bool validate(const Settings& s) {
  if (s.readIntervalMs < 1000 || s.readIntervalMs > 600000) {
    snprintf(error, sizeof error, "read_interval_ms must be 1000..600000");
    return false;
  }
  if (s.uploadIntervalMs < 5000 || s.uploadIntervalMs > 3600000) {
    snprintf(error, sizeof error, "upload_interval_ms must be 5000..3600000");
    return false;
  }
  if (s.staleAfterMs < 2 * s.readIntervalMs || s.staleAfterMs > 3600000) {
    snprintf(error, sizeof error,
             "stale_after_ms must be >= 2x read_interval_ms");
    return false;
  }
  return true;
}

// Missing field = keep the current value. Wrong type = reject.
bool readField(const JsonDocument& doc, const char* key, uint32_t& out) {
  JsonVariantConst v = doc[key];
  if (v.isNull()) return true;
  if (!v.is<uint32_t>()) {
    snprintf(error, sizeof error, "%s must be a positive integer", key);
    return false;
  }
  out = v.as<uint32_t>();
  return true;
}

void save(const Settings& s) {
  Preferences prefs;
  if (!prefs.begin(NVS_NAMESPACE, false)) return;
  prefs.putUInt("ver", s.version);
  prefs.putUInt("read", s.readIntervalMs);
  prefs.putUInt("upload", s.uploadIntervalMs);
  prefs.putUInt("stale", s.staleAfterMs);
  prefs.end();
}

}  // namespace

namespace settings {

void begin() {
  current.version = 0;
  current.readIntervalMs = DEFAULT_READ_INTERVAL_MS;
  current.uploadIntervalMs = DEFAULT_UPLOAD_INTERVAL_MS;
  current.staleAfterMs = DEFAULT_STALE_AFTER_MS;

  Preferences prefs;
  if (prefs.begin(NVS_NAMESPACE, true)) {  // fails harmlessly on first boot
    if (prefs.isKey("ver")) {
      Settings saved;
      saved.version = prefs.getUInt("ver", 0);
      saved.readIntervalMs = prefs.getUInt("read", current.readIntervalMs);
      saved.uploadIntervalMs = prefs.getUInt("upload", current.uploadIntervalMs);
      saved.staleAfterMs = prefs.getUInt("stale", current.staleAfterMs);
      if (validate(saved)) current = saved;
    }
    prefs.end();
  }
  error[0] = '\0';
  Serial.printf("[cfg] v%lu read=%lums upload=%lums stale=%lums\n",
                (unsigned long)current.version,
                (unsigned long)current.readIntervalMs,
                (unsigned long)current.uploadIntervalMs,
                (unsigned long)current.staleAfterMs);
}

const Settings& get() { return current; }

ApplyResult applyJson(const char* json, size_t len) {
  JsonDocument doc;
  if (deserializeJson(doc, json, len)) {
    snprintf(error, sizeof error, "settings are not valid JSON");
    return ApplyResult::REJECTED;
  }
  JsonVariantConst ver = doc["version"];
  if (!ver.is<uint32_t>() || ver.as<uint32_t>() == 0) {
    snprintf(error, sizeof error, "version must be a positive integer");
    return ApplyResult::REJECTED;
  }
  if (ver.as<uint32_t>() == current.version) return ApplyResult::UNCHANGED;

  Settings next = current;
  next.version = ver.as<uint32_t>();
  if (!readField(doc, "read_interval_ms", next.readIntervalMs) ||
      !readField(doc, "upload_interval_ms", next.uploadIntervalMs) ||
      !readField(doc, "stale_after_ms", next.staleAfterMs) ||
      !validate(next)) {
    return ApplyResult::REJECTED;
  }

  current = next;
  error[0] = '\0';
  save(current);
  return ApplyResult::APPLIED;
}

const char* lastError() { return error; }

}  // namespace settings
