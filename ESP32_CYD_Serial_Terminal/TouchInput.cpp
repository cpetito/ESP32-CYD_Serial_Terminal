// TouchInput.cpp
#include "TouchInput.h"

void TouchInput::begin(int16_t screenW, int16_t screenH) {
  screenW_ = screenW;
  screenH_ = screenH;

  touchscreen_.begin(&touchSPI_);
  // Rotation 1 is this library's passthrough case (xraw=x, yraw=y - see
  // its update()), keeping "raw" here equal to the true ADC reading, so
  // our own TOUCH_SWAP_XY/INVERT_*/calibration constants below still mean
  // exactly what they meant against the previous hardware-SPI library.
  touchscreen_.setRotation(1);
}

bool TouchInput::getPoint(int16_t &x, int16_t &y) {
  // tirqTouched() is a cheap flag check (set by the XPT2046's IRQ pin via
  // interrupt); touched() is what actually issues SPI traffic and checks
  // pressure. Checking both, in this order, avoids that traffic entirely
  // while nothing is pressed.
  if (!touchscreen_.tirqTouched() || !touchscreen_.touched(&touchSPI_)) {
    return false;
  }

  TS_Point p = touchscreen_.getPoint(&touchSPI_);

  // Which raw axis feeds the screen's X vs Y, and its calibration range -
  // see the TOUCH_SWAP_XY comment in Config.h for why this board needs it.
#if TOUCH_SWAP_XY
  long rawX = p.y, rawXMin = TOUCH_RAW_Y_MIN, rawXMax = TOUCH_RAW_Y_MAX;
  long rawY = p.x, rawYMin = TOUCH_RAW_X_MIN, rawYMax = TOUCH_RAW_X_MAX;
#else
  long rawX = p.x, rawXMin = TOUCH_RAW_X_MIN, rawXMax = TOUCH_RAW_X_MAX;
  long rawY = p.y, rawYMin = TOUCH_RAW_Y_MIN, rawYMax = TOUCH_RAW_Y_MAX;
#endif

#if TOUCH_INVERT_X
  long mx = map(rawX, rawXMin, rawXMax, screenW_, 0);
#else
  long mx = map(rawX, rawXMin, rawXMax, 0, screenW_);
#endif
#if TOUCH_INVERT_Y
  long my = map(rawY, rawYMin, rawYMax, screenH_, 0);
#else
  long my = map(rawY, rawYMin, rawYMax, 0, screenH_);
#endif

  mx = constrain(mx, 0, screenW_ - 1);
  my = constrain(my, 0, screenH_ - 1);

  x = (int16_t)mx;
  y = (int16_t)my;

#if TOUCH_DEBUG_SERIAL
  Serial.printf("touch raw=(%d,%d) mapped=(%d,%d)\n", p.x, p.y, x, y);
#endif

  return true;
}
