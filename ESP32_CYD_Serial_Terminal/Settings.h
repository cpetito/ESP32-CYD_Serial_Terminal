// Settings.h
// Wraps the ESP32 Preferences (NVS) library for persisting the selected
// baud rate and a monotonically increasing SD session counter.

#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include "Config.h"

class Settings {
public:
  void begin();

  uint32_t getBaudRate() const { return baudRate_; }
  void setBaudRate(uint32_t baud);

  // Returns the next session number and persists the increment, so every
  // recording gets a unique, ever-increasing file name even across reboots.
  uint32_t nextSessionNumber();

private:
  Preferences prefs_;
  uint32_t baudRate_ = DEFAULT_BAUD_RATE;
};
