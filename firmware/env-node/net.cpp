#include "net.h"

#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#include "config.h"

namespace {

uint32_t lastConnectAttemptMs = 0;
bool wasConnected = false;

void startConnect(uint32_t nowMs) {
  Serial.printf("[net] connecting to %s\n", WIFI_SSID);
  WiFi.disconnect();
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  lastConnectAttemptMs = nowMs;
}

// One HTTP request. body == nullptr means GET. Fills `response` on GET.
int request(const char* path, const char* body, String* response) {
  if (!net::connected()) return net::ERR_OFFLINE;

  const String url = String(API_BASE_URL) + path;
  // Clients are declared before `http` so they outlive it (destroyed last).
  WiFiClient plainClient;
  WiFiClientSecure tlsClient;
  HTTPClient http;

  bool ok;
  if (url.startsWith("https://")) {
    // Demo setting: encrypts traffic but skips certificate checks.
    // For a real deployment, use tlsClient.setCACert(<root CA>).
    tlsClient.setInsecure();
    ok = http.begin(tlsClient, url);
  } else {
    ok = http.begin(plainClient, url);
  }
  if (!ok) return net::ERR_BEGIN;

  http.setConnectTimeout(HTTP_TIMEOUT_MS);
  http.setTimeout(HTTP_TIMEOUT_MS);
  if (strlen(API_KEY) > 0) {
    http.addHeader("Authorization", String("Bearer ") + API_KEY);
  }

  int code;
  if (body) {
    http.addHeader("Content-Type", "application/json");
    code = http.POST((uint8_t*)body, strlen(body));
  } else {
    code = http.GET();
    if (code > 0 && response) *response = http.getString();
  }
  if (code < 0) {
    Serial.printf("[net] %s %s failed: %s\n", body ? "POST" : "GET",
                  url.c_str(), HTTPClient::errorToString(code).c_str());
  }
  http.end();
  return code;
}

}  // namespace

namespace net {

void begin() {
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  startConnect(millis());
  configTime(0, 0, NTP_SERVER);  // UTC; SNTP syncs once Wi-Fi is up
}

void update(uint32_t nowMs) {
  const bool up = connected();
  if (up != wasConnected) {
    if (up) {
      Serial.printf("[net] connected, ip=%s rssi=%d\n",
                    WiFi.localIP().toString().c_str(), WiFi.RSSI());
    } else {
      Serial.println("[net] Wi-Fi lost");
    }
    wasConnected = up;
  }
  if (!up && nowMs - lastConnectAttemptMs >= WIFI_RETRY_MS) {
    startConnect(nowMs);
  }
}

bool connected() { return WiFi.status() == WL_CONNECTED; }

int rssi() { return connected() ? WiFi.RSSI() : 0; }

time_t epochNow() {
  const time_t now = time(nullptr);
  return now < 1700000000 ? 0 : now;  // not synced yet (before Nov 2023)
}

void formatIso(time_t epoch, char* buf, size_t len) {
  struct tm utc;
  gmtime_r(&epoch, &utc);
  strftime(buf, len, "%Y-%m-%dT%H:%M:%SZ", &utc);
}

int postJson(const char* path, const char* body) {
  return request(path, body, nullptr);
}

int getJson(const char* path, String& responseBody) {
  return request(path, nullptr, &responseBody);
}

}  // namespace net
