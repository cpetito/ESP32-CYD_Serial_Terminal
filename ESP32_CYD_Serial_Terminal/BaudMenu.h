// BaudMenu.h
// Full-screen overlay grid for touch-selecting the monitored baud rate.

#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>
#include "Config.h"
#include "UITypes.h"

// Returned by BaudMenu::hitTest() when the Cancel button was tapped.
#define BAUD_MENU_CANCEL 0xFFFFFFFFu

class BaudMenu {
public:
  void draw(TFT_eSPI &tft, uint32_t currentBaud);

  // Returns the tapped baud rate, 0 if the tap missed every option, or
  // BAUD_MENU_CANCEL if the Cancel button was tapped.
  uint32_t hitTest(int16_t x, int16_t y) const;

private:
  Rect optionRect_[BAUD_RATE_OPTIONS_COUNT];
  Rect cancelRect_;
};
