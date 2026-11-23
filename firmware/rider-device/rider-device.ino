#include "button.h"
#include "haptic.h"
#include "display.h"

void setup() {
  Serial.begin(115200);
  Button::begin(0);
  Haptic::begin(25);
  Display::begin(21, 22);
}

void loop() {
  Button::update();
  Haptic::update();
  Display::update();

  if (Button::doublePressed()) {
    Display::showStatus("Report sent");
    Haptic::pulse();
  }
  if (Button::singlePressed()) {
    if (Display::warningActive()) {
      Display::clearWarning();
      Haptic::stop();
      Display::showStatus("Acknowledged");
    }
  }
}