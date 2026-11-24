class Button {
 public:
  Button(uint8_t pin);
  void begin();
  void update();
  bool doublePress();
  bool singlePress();
 private:
  uint8_t _pin;
  unsigned long _lastPress;
  uint8_t _count;
};
