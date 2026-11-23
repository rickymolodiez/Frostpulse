// Wi-Fi connection management + HTTP JSON calls to the FrostPulse cloud (ESP32).
#pragma once
#include <Arduino.h>
#include <time.h>

namespace net {

// Returned instead of an HTTP status when Wi-Fi is down or the request could
// not be set up. HTTPClient's own errors are -1..-11.
constexpr int ERR_OFFLINE = -100;
constexpr int ERR_BEGIN = -101;

void begin();                  // start Wi-Fi + NTP; does not block
void update(uint32_t nowMs);   // call every loop(); reconnects when needed
bool connected();
int rssi();                    // dBm, 0 when disconnected

// Current UTC epoch seconds, or 0 until NTP has synced.
time_t epochNow();
// Format epoch seconds as "YYYY-MM-DDTHH:MM:SSZ".
void formatIso(time_t epoch, char* buf, size_t len);

// POST a JSON body / GET a JSON document at API_BASE_URL + path.
// Return the HTTP status code, or a negative error code.
// Each call blocks for up to HTTP_TIMEOUT_MS.
int postJson(const char* path, const char* body);
int getJson(const char* path, String& responseBody);

}  // namespace net
