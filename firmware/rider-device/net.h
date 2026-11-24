#pragma once
#include <Arduino.h>

namespace Net {
  enum ReportResult : uint8_t { REPORT_OK, REPORT_NO_LOCATION, REPORT_FAILED };

  void begin();
  void update();                 // call every loop(): Wi-Fi reconnect + alert polling

  bool connected();
  ReportResult sendReport();     // blocking HTTP call (short timeout)

  bool hasAlert();               // true while an unacknowledged alert is pending
  const char* alertMessage();
  bool sendAck();                // acknowledges the pending alert; clears it on success
}