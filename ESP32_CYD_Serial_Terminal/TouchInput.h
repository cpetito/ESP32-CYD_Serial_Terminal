// TouchInput.h
// Thin wrapper around the XPT2046 resistive touch controller, which lives
// on its own bit-banged SPI bus separate from the TFT's VSPI bus. Maps raw
// ADC readings to screen coordinates for the current rotation.

#pragma once

#include <Arduino.h>
#include <SPI.h>
#include <XPT2046_Touchscreen.h>
#include "Config.h"

class TouchInput {
public:
  void begin(int16_t screenW, int16_t screenH);

  // Returns true if the panel is currently being touched, with (x, y)
  // mapped into screen coordinates (already accounting for rotation).
  bool getPoint(int16_t &x, int16_t &y);

private:
  SPIClass touchSPI_ = SPIClass(HSPI);
  XPT2046_Touchscreen ts_ = XPT2046_Touchscreen(TOUCH_CS_PIN, TOUCH_IRQ_PIN);
  int16_t screenW_ = 320;
  int16_t screenH_ = 240;
};
