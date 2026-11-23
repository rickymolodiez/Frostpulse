// Runtime settings that the cloud can change (remote configuration).
// Defaults come from config.h; accepted updates are saved to flash (NVS).
#pragma once
#include <Arduino.h>

struct Settings {
  uint32_t version = 0;  // 0 = built-in defaults, never set by the cloud
  uint32_t readIntervalMs;
  uint32_t uploadIntervalMs;
  uint32_t staleAfterMs;
};

enum class ApplyResult : uint8_t { UNCHANGED, APPLIED, REJECTED };

namespace settings {

void begin();  // load saved settings, or defaults
const Settings& get();

// Apply a settings JSON document from the cloud. Invalid documents are
// rejected as a whole and the current settings are kept.
ApplyResult applyJson(const char* json, size_t len);

// Why the last update was rejected ("" if the last one was fine).
const char* lastError();

}  // namespace settings
