#include "net.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>        // v7
#include <time.h>
#include "config.h"             // WIFI_SSID, WIFI_PASSWORD, API_BASE_URL, DEVICE_ID, optional API_KEY

static const uint16_t HTTP_TIMEOUT_MS = 2500;
static const uint32_t WIFI_RETRY_MS   = 5000;
static const uint32_t ALERT_POLL_MS   = 3000;

static WiFiClient       plainClient;
static WiFiClientSecure secureClient;

static unsigned long lastWifiAttempt = 0;
static unsigned long lastPoll = 0;
static bool wifiStarted = false;

static char     bootId[9];
static uint32_t reportSeq = 1;   // bumped only after a definitive answer
static uint32_t ackSeq = 1;

static bool     alertPending = false;
static uint32_t alertId = 0;
static char     alertMsg[32] = "";

// ISO 8601 UTC in quotes, or the literal null when NTP hasn't synced yet.
static void nowIsoOrNull(char* out, size_t n) {
  time_t t = time(nullptr);
  if (t < 1700000000) { snprintf(out, n, "null"); return; }   // not synced
  struct tm tm;
  gmtime_r(&t, &tm);
  snprintf(out, n, "\"%04d-%02d-%02dT%02d:%02d:%02dZ\"",
           tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
}

// Returns the HTTP status (<=0 on transport failure). Parses any JSON body into `doc`,
// including on 4xx, because a 422 carries {"error":"no_location"}.
static int request(const char* method, const String& path, const char* body, JsonDocument& doc) {
  if (WiFi.status() != WL_CONNECTED) return -1;

  String url = String(API_BASE_URL) + path;
  HTTPClient http;
  http.setTimeout(HTTP_TIMEOUT_MS);
  http.setConnectTimeout(HTTP_TIMEOUT_MS);

  bool ok;
  if (url.startsWith("https")) {
    secureClient.setInsecure();            // DEV ONLY: no certificate check
    ok = http.begin(secureClient, url);
  } else {
    ok = http.begin(plainClient, url);
  }
  if (!ok) return -2;

#ifdef API_KEY
  http.addHeader("Authorization", String("Bearer ") + API_KEY);
#endif

  int code;
  if (strcmp(method, "POST") == 0) {
    http.addHeader("Content-Type", "application/json");
    code = http.POST((uint8_t*)body, strlen(body));
  } else {
    code = http.GET();
  }

  if (code > 0) deserializeJson(doc, http.getStream());   // empty body leaves doc null
  http.end();
  return code;
}

static void pollAlert() {
  String path = String("/api/alerts?device_id=") + DEVICE_ID;

  JsonDocument doc;
  int code = request("GET", path, nullptr, doc);
  if (code != 200) return;

  JsonVariant a = doc["alert"];
  if (a.isNull()) return;

  alertId = a["id"] | 0;
  strncpy(alertMsg, a["message"] | "Hazard ahead", sizeof(alertMsg) - 1);
  alertMsg[sizeof(alertMsg) - 1] = '\0';
  alertPending = true;
}

namespace Net {
  void begin() {
    snprintf(bootId, sizeof(bootId), "%08x", (unsigned)esp_random());

    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    wifiStarted = true;
    lastWifiAttempt = millis();

    configTime(0, 0, "pool.ntp.org", "time.nist.gov");   // for pressed_at
  }

  void update() {
    unsigned long now = millis();

    if (WiFi.status() != WL_CONNECTED) {
      if (wifiStarted && now - lastWifiAttempt >= WIFI_RETRY_MS) {
        lastWifiAttempt = now;
        WiFi.disconnect();
        WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
      }
      return;
    }

    // Don't poll while an alert is pending, so it isn't overwritten before the ack
    if (!alertPending && now - lastPoll >= ALERT_POLL_MS) {
      lastPoll = now;
      pollAlert();
    }
  }

  bool connected() { return WiFi.status() == WL_CONNECTED; }

  ReportResult sendReport() {
    char ts[32];
    nowIsoOrNull(ts, sizeof(ts));

    char body[220];
    snprintf(body, sizeof(body),
             "{\"device_id\":\"%s\",\"boot_id\":\"%s\",\"seq\":%lu,"
             "\"pressed_at\":%s,\"age_ms\":0,\"rssi\":%d,\"uptime_ms\":%lu}",
             DEVICE_ID, bootId, (unsigned long)reportSeq, ts, WiFi.RSSI(), millis());

    JsonDocument doc;
    int code = request("POST", "/api/reports", body, doc);

    if (code == 200 || code == 201) { reportSeq++; return REPORT_OK; }
    if (code == 422) {
      reportSeq++;                                   // definitive answer: next press is a new report
      const char* err = doc["error"] | "";
      if (strcmp(err, "no_location") == 0) return REPORT_NO_LOCATION;
      return REPORT_FAILED;
    }
    if (code == 400) { reportSeq++; return REPORT_FAILED; }   // invalid: retrying won't help
    return REPORT_FAILED;                            // temporary: keep seq so a retry dedupes
  }

  bool hasAlert() { return alertPending; }
  const char* alertMessage() { return alertMsg; }

  bool sendAck() {
    if (!alertPending) return false;

    char body[140];
    snprintf(body, sizeof(body),
             "{\"device_id\":\"%s\",\"alert_id\":%lu,\"boot_id\":\"%s\",\"seq\":%lu}",
             DEVICE_ID, (unsigned long)alertId, bootId, (unsigned long)ackSeq);

    JsonDocument doc;
    int code = request("POST", "/api/acks", body, doc);

    bool done = (code == 200 || code == 201 ||                // stored / duplicate
                 code == 404 || code == 409 || code == 410);  // unknown or already acked
    if (!done) return false;                                  // temporary: keep the warning

    ackSeq++;
    alertPending = false;
    return true;
  }
}