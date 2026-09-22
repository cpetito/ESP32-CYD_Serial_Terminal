// Settings.cpp
#include "Settings.h"

void Settings::begin() {
  prefs_.begin(PREFS_NAMESPACE, false);
  baudRate_ = prefs_.getUInt(PREFS_KEY_BAUD, DEFAULT_BAUD_RATE);
}

void Settings::setBaudRate(uint32_t baud) {
  baudRate_ = baud;
  prefs_.putUInt(PREFS_KEY_BAUD, baud);
}

uint32_t Settings::nextSessionNumber() {
  uint32_t n = prefs_.getUInt(PREFS_KEY_SESSION_NUM, 0) + 1;
  prefs_.putUInt(PREFS_KEY_SESSION_NUM, n);
  return n;
}
