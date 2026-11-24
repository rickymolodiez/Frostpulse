// display.cpp
#include "display.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

static const uint8_t SCREEN_W = 128;
static const uint8_t SCREEN_H = 64;
static const uint8_t I2C_ADDR = 0x3C;

static Adafruit_SSD1306 oled(SCREEN_W, SCREEN_H, &Wire, -1);

enum Mode : uint8_t { MODE_IDLE, MODE_STATUS, MODE_WARNING };

static Mode mode = MODE_IDLE;
static bool dirty = true;
static bool ready = false;
static bool wifiOk = false;

static char statusMsg[32] = "";
static char warningMsg[32] = "";
static unsigned long statusStart = 0;
static uint16_t statusDuration = 0;

static void drawIdle() {
  oled.setTextSize(2);
  oled.setCursor(0, 0);
  oled.println("FrostPulse");
  oled.setTextSize(1);
  oled.setCursor(0, 28);
  oled.println("Rider device ready");
  oled.setCursor(0, 48);
  oled.print("Wi-Fi: ");
  oled.print(wifiOk ? "connected" : "offline");
}

static void drawStatus() {
  oled.setTextSize(1);
  oled.setCursor(0, 0);
  oled.println("STATUS");
  oled.drawFastHLine(0, 10, SCREEN_W, SSD1306_WHITE);
  oled.setTextSize(2);
  oled.setCursor(0, 20);
  oled.println(statusMsg);
}

static void drawWarning() {
  oled.fillRect(0, 0, SCREEN_W, 16, SSD1306_WHITE);
  oled.setTextColor(SSD1306_BLACK);
  oled.setTextSize(2);
  oled.setCursor(4, 1);
  oled.print("WARNING");
  oled.setTextColor(SSD1306_WHITE);
  oled.setTextSize(1);
  oled.setCursor(0, 24);
  oled.println(warningMsg);
  oled.setCursor(0, 54);
  oled.print("Press to acknowledge");
}

static void render() {
  oled.clearDisplay();
  oled.setTextColor(SSD1306_WHITE);
  switch (mode) {
    case MODE_IDLE:    drawIdle();    break;
    case MODE_STATUS:  drawStatus();  break;
    case MODE_WARNING: drawWarning(); break;
  }
  oled.display();
  dirty = false;
}

namespace Display {
  bool begin(uint8_t sda, uint8_t scl, int8_t rstPin) {
    if (rstPin >= 0) {                      // some boards need a manual OLED reset
      pinMode(rstPin, OUTPUT);
      digitalWrite(rstPin, LOW);
      delay(20);
      digitalWrite(rstPin, HIGH);
    }
    Wire.begin(sda, scl);
    ready = oled.begin(SSD1306_SWITCHCAPVCC, I2C_ADDR);
    if (ready) {
      dirty = true;
      render();
    }
    return ready;
  }

  void update() {
    if (!ready) return;

    // temporary status  back to idle
    if (mode == MODE_STATUS && millis() - statusStart >= statusDuration) {
      mode = MODE_IDLE;
      dirty = true;
    }
    if (dirty) render();
  }

  void showStatus(const char* msg, uint16_t durationMs) {
    if (mode == MODE_WARNING) return;       // warnings take priority
    strncpy(statusMsg, msg, sizeof(statusMsg) - 1);
    statusMsg[sizeof(statusMsg) - 1] = '\0';
    statusStart = millis();
    statusDuration = durationMs;
    mode = MODE_STATUS;
    dirty = true;
  }

  void showWarning(const char* msg) {
    strncpy(warningMsg, msg, sizeof(warningMsg) - 1);
    warningMsg[sizeof(warningMsg) - 1] = '\0';
    mode = MODE_WARNING;
    dirty = true;
  }

  void clearWarning() {
    if (mode != MODE_WARNING) return;
    mode = MODE_IDLE;
    dirty = true;
  }

  bool warningActive() { return mode == MODE_WARNING; }

  void setWifiConnected(bool connected) {
    if (wifiOk == connected) return;
    wifiOk = connected;
    if (mode == MODE_IDLE) dirty = true;
  }
}