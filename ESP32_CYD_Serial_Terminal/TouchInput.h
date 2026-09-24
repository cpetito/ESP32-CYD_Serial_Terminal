// TouchInput.h
// Bit-banged (software) SPI driver for the XPT2046 resistive touch
// controller. This is deliberate, not a fallback: the ESP32 classic only
// has two general-purpose hardware SPI peripherals (HSPI/VSPI), and this
// board needs three independent SPI buses (TFT, touch, microSD - their
// SCLK/MOSI/MISO pins genuinely differ, so they can't share one hardware
// peripheral). Bit-banging touch - the lowest-bandwidth of the three, and
// fine with software timing at typical poll rates - frees a full hardware
// peripheral for the microSD card's exclusive use. See SDLogger.cpp.
//
// Maps raw ADC readings to screen coordinates for the current rotation.

#pragma once

#include <Arduino.h>
#include "Config.h"

class TouchInput {
public:
  void begin(int16_t screenW, int16_t screenH);

  // Returns true if the panel is currently being touched, with (x, y)
  // mapped into screen coordinates (already accounting for rotation).
  bool getPoint(int16_t &x, int16_t &y);

private:
  int16_t screenW_ = 320;
  int16_t screenH_ = 240;
};
