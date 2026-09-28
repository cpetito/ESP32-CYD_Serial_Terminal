// TxMenu.h
// Full-screen overlay grid for touch-selecting a canned message to
// transmit out MONITOR_TX_PIN. Mirrors BaudMenu's layout/hit-test
// pattern; see Config.h's TX_MESSAGES for the actual message list.

#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>
#include "Config.h"
#include "UITypes.h"

class TxMenu {
public:
  void draw(TFT_eSPI &tft);

  // Returns the tapped message's index into TX_MESSAGES, or -1 if the
  // tap missed every button or hit Cancel - the caller treats both the
  // same way (just close the menu), so there's no separate cancel
  // sentinel here the way BaudMenu needs one to distinguish "cancel"
  // from "a valid baud rate of 0".
  int hitTest(int16_t x, int16_t y) const;

private:
  Rect optionRect_[TX_MESSAGES_COUNT];
  Rect cancelRect_;
};
