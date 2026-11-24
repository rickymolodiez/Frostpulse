#include <button.h>

namespace {

	uint8_t pin;
	bool lastState = HIGH;
	unsigned long lastChange = 0;
	unsigned long lastRelease =0;
	uint8_t clicks = 0;
	bool single = false;
	bool dbl = false;

	const unsigned long DEBOUNCE_MS = 30;
	const unsigned long DOUBLE_WINDOW_MS = 350;
}

namespace Button {

	void begin(uint8_t pin){
		pin = pin;
		pinMode(pin, INPUT_PULLUP);
	}

	void update(){
		bool state = digitalRead(pin);
		unsigned long now = millis();

		if(state != lastState && now - lastChange > DEBOUNCE_MS){
			lastChange = now;
			lastState = state;
			if (state == HIGH)
			{
				clicks++;
				lastRelease = now;
			}
		}

		if (clicks > 0 && now - lastRelease > DOUBLE_WINDOW_MS) {
			if(clicks == 1) single = true;
			else if (clicks >= 2) dbl = true;
			clicks = 0;
		}
	}

	bool singlePress() {
		bool r = single;
		single = false;
		return r;
	}
	bool doublePress() {
		bool r = dbl;
		dbl = false;
		return r;
}
