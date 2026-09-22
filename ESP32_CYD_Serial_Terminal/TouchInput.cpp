// TouchInput.cpp
#include "TouchInput.h"

void TouchInput::begin(int16_t screenW, int16_t screenH) {
  screenW_ = screenW;
  screenH_ = screenH;
  touchSPI_.begin(TOUCH_CLK_PIN, TOUCH_MISO_PIN, TOUCH_MOSI_PIN, TOUCH_CS_PIN);
  ts_.begin(touchSPI_);
  // Rotation is handled manually via the map() below rather than the
  // library's own setRotation(), since not every XPT2046_Touchscreen fork
  // implements it.
}

bool TouchInput::getPoint(int16_t &x, int16_t &y) {
  if (!ts_.touched()) return false;

  TS_Point p = ts_.getPoint();

  // Calibration mapping for landscape rotation 1. If touches land mirrored
  // or on the wrong axis on your particular panel, swap p.x/p.y here or
  // flip the destination ranges (0..W / 0..H) - see README.md.
  long mx = map(p.x, TOUCH_RAW_X_MIN, TOUCH_RAW_X_MAX, 0, screenW_);
  long my = map(p.y, TOUCH_RAW_Y_MIN, TOUCH_RAW_Y_MAX, 0, screenH_);

  mx = constrain(mx, 0, screenW_ - 1);
  my = constrain(my, 0, screenH_ - 1);

  x = (int16_t)mx;
  y = (int16_t)my;
  return true;
}
