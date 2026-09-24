// TouchInput.h
// Drives the XPT2046 resistive touch controller via SoftSPI (bit-banged,
// software-only SPI) rather than a hardware SPI peripheral. This is
// deliberate, not a fallback: the ESP32 classic only has two
// general-purpose hardware SPI peripherals (HSPI/VSPI), and this board
// needs three independent SPI buses - TFT, touch, and microSD all have
// genuinely different SCLK/MOSI/MISO pins, confirmed by testing, so they
// can't share one hardware peripheral. Bit-banging touch - the
// lowest-bandwidth of the three, and fine with software timing at
// typical poll rates - frees a whole hardware peripheral for the microSD
// card's exclusive use (see SDLogger.h).
//
// SoftSPI + XPT2046_TouchscreenSOFTSPI are vendored from RandomNerdTutorials'
// CYD example (see their own headers for source/attribution) rather than
// hand-rolled, since the exact XPT2046 bit timing is a hardware detail
// that's easy to get subtly wrong without a scope to verify against.
//
// Maps raw ADC readings to screen coordinates for the current rotation.

#pragma once

#include <Arduino.h>
#include "Config.h"
#include "SoftSPI.h"
#include "XPT2046_TouchscreenSOFTSPI.h"

class TouchInput {
public:
  void begin(int16_t screenW, int16_t screenH);

  // Returns true if the panel is currently being touched, with (x, y)
  // mapped into screen coordinates (already accounting for rotation).
  bool getPoint(int16_t &x, int16_t &y);

private:
  SoftSPI touchSPI_ = SoftSPI(TOUCH_MOSI_PIN, TOUCH_MISO_PIN, TOUCH_CLK_PIN);
  XPT2046_TouchscreenSOFTSPI touchscreen_ = XPT2046_TouchscreenSOFTSPI(TOUCH_CS_PIN, TOUCH_IRQ_PIN);
  int16_t screenW_ = 320;
  int16_t screenH_ = 240;
};
