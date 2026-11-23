#include "display.h"

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Wire.h>

#include "config.h"

namespace {
Adafruit_SSD1306 oled(OLED_WIDTH, OLED_HEIGHT, &Wire, OLED_RST);
bool present = false;
}  // namespace

namespace display {

bool begin() {
  Wire.begin(OLED_SDA, OLED_SCL);
  present = oled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
  if (!present) {
    Serial.println("[display] SSD1306 not found, running headless");
    return false;
  }
  oled.setTextColor(SSD1306_WHITE);
  oled.clearDisplay();
  oled.display();
  return true;
}

void message(const char* line1, const char* line2) {
  if (!present) return;
  oled.clearDisplay();
  oled.setTextSize(1);
  oled.setCursor(0, 20);
  oled.println(line1);
  oled.setCursor(0, 34);
  oled.println(line2);
  oled.display();
}

// Layout (21 columns x 8 rows of 6x8 px at text size 1):
//   env-01  cfg v3   WiFi
//   23.0 C            (size 2)
//   RH 45%  DP 10.5C
//   SENSOR OK
//   UP 201  Q0
void render(const Reading& r, SensorStatus s, uint32_t readingAgeMs,
            const NodeStatus& node) {
  if (!present) return;
  char buf[24];
  oled.clearDisplay();

  // Header: node id, settings version, Wi-Fi
  oled.setTextSize(1);
  oled.setCursor(0, 0);
  oled.print(NODE_ID);
  oled.setCursor(48, 0);
  snprintf(buf, sizeof buf, "cfg v%lu%s", (unsigned long)node.configVersion,
           node.configError ? "!" : "");
  oled.print(buf);
  oled.setCursor(OLED_WIDTH - 4 * 6, 0);
  oled.print(node.wifiUp ? "WiFi" : "----");

  // Temperature, large
  oled.setTextSize(2);
  oled.setCursor(0, 12);
  if (s == SensorStatus::NO_DATA) {
    oled.print("--.- C");
  } else {
    snprintf(buf, sizeof buf, "%.1f C", r.tempC);
    oled.print(buf);
  }

  // Humidity + dew point
  oled.setTextSize(1);
  oled.setCursor(0, 32);
  if (s == SensorStatus::NO_DATA) {
    oled.print("RH --%  DP --");
  } else {
    snprintf(buf, sizeof buf, "RH %.0f%%  DP %.1fC", r.humidity, r.dewPointC);
    oled.print(buf);
  }

  // Sensor status
  oled.setCursor(0, 44);
  switch (s) {
    case SensorStatus::NO_DATA:
      oled.print("SENSOR NO DATA");
      break;
    case SensorStatus::STALE:
      snprintf(buf, sizeof buf, "SENSOR STALE %lus",
               (unsigned long)(readingAgeMs / 1000));
      oled.print(buf);
      break;
    case SensorStatus::OK:
      // DHT11 is not reliable outside its rated range (e.g. below 0 C).
      oled.print(dht_sensor::inRatedRange(r) ? "SENSOR OK"
                                             : "SENSOR OK (LOW CONF)");
      break;
  }

  // Upload state + queue depth
  oled.setCursor(0, 56);
  if (!node.wifiUp) {
    snprintf(buf, sizeof buf, "OFFLINE  Q%u", node.queued);
  } else if (node.lastHttpCode == 0) {
    snprintf(buf, sizeof buf, "UP --  Q%u", node.queued);
  } else if (node.lastHttpCode >= 200 && node.lastHttpCode < 300) {
    snprintf(buf, sizeof buf, "UP %d  Q%u", node.lastHttpCode, node.queued);
  } else {
    snprintf(buf, sizeof buf, "UP ERR %d  Q%u", node.lastHttpCode, node.queued);
  }
  oled.print(buf);

  oled.display();
}

}  // namespace display
