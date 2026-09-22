// UITypes.h
// Small shared types used by both the rendering and touch-handling code so
// button hit-boxes are defined once and stay in sync with what's drawn.

#pragma once

#include <Arduino.h>

struct Rect {
  int16_t x, y, w, h;
  bool contains(int16_t px, int16_t py) const {
    return px >= x && px < (x + w) && py >= y && py < (y + h);
  }
};
