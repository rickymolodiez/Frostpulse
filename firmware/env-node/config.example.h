// FrostPulse env-node configuration (TTGO ESP32 with built-in OLED + DHT11).
//
// Copy this file to config.h (same folder) and fill in your values.
// config.h is gitignored so Wi-Fi passwords and API keys stay out of the repo.
#pragma once

// ---- Identity -------------------------------------------------------------
#define NODE_ID          "env-01"      // unique per node, used by the cloud
#define NODE_LAT         45.497000     // fixed install location of this node
#define NODE_LON         -73.578900

// ---- Wi-Fi ----------------------------------------------------------------
#define WIFI_SSID        "your-ssid"
#define WIFI_PASSWORD    "your-password"
#define WIFI_RETRY_MS    10000         // how often to retry when disconnected

// ---- Cloud API ------------------------------------------------------------
#define API_BASE_URL     "http://192.168.1.50:3000"  // no trailing slash
#define READINGS_PATH    "/api/readings"
#define SETTINGS_PATH    "/api/settings/" NODE_ID
#define API_KEY          ""            // sent as "Authorization: Bearer <key>" if set
#define HTTP_TIMEOUT_MS  5000
#define NTP_SERVER       "pool.ntp.org"
#define SETTINGS_POLL_MS 60000         // how often to ask the cloud for new settings

// ---- Timing defaults ------------------------------------------------------
// The cloud can override these three at runtime (GET SETTINGS_PATH). The
// last accepted values are saved in flash and survive a reboot.
#define DEFAULT_READ_INTERVAL_MS   2000   // DHT11 needs >= 1000 ms between reads
#define DEFAULT_UPLOAD_INTERVAL_MS 30000  // one observation queued per interval
#define DEFAULT_STALE_AFTER_MS     10000  // reading older than this = "stale"

#define POST_RETRY_MS    5000          // retry delay after a failed upload
#define QUEUE_SIZE       30            // observations kept while offline
#define DISPLAY_REFRESH_MS 1000

// ---- Sensor (DHT11) -------------------------------------------------------
#define DHT_PIN          13            // free on TTGO LoRa32 V1.3 (not used by OLED/LoRa)
#define DHT_TYPE         DHT11
// Garbage filter: reads outside these limits are rejected as sensor errors.
#define TEMP_MIN_C       -20.0f
#define TEMP_MAX_C       60.0f
// DHT11 rated range. Readings outside it are still sent but flagged with
// "in_rated_range": false (the DHT11 is unreliable below 0 C).
#define RATED_TEMP_MIN_C 0.0f
#define RATED_TEMP_MAX_C 50.0f
#define RATED_RH_MIN     20.0f
#define RATED_RH_MAX     90.0f
// Reject a single read that jumps more than this from the last fresh value.
#define MAX_TEMP_JUMP_C  10.0f

// ---- Built-in OLED (SSD1306 128x64, I2C) ----------------------------------
// Our board is TTGO LoRa32 V1.3: SDA 4, SCL 15, RST 16 (values below).
// (For a V2.1 / T3 v1.6 board instead: SDA 21, SCL 22, RST -1.)
#define OLED_WIDTH       128
#define OLED_HEIGHT      64
#define OLED_ADDR        0x3C
#define OLED_SDA         4
#define OLED_SCL         15
#define OLED_RST         16
