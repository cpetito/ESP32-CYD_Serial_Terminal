// BaudMenu.cpp
#include "BaudMenu.h"

void BaudMenu::draw(TFT_eSPI &tft, uint32_t currentBaud) {
  int16_t w = tft.width();
  int16_t h = tft.height();

  tft.fillScreen(COLOR_OVERLAY_BG);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(COLOR_STATUS_TEXT, COLOR_OVERLAY_BG);
  tft.setTextFont(2);
  tft.setTextSize(1);
  tft.drawString("Select Baud Rate", w / 2, 16);

  const int cols = 3;
  const int rows = (BAUD_RATE_OPTIONS_COUNT + cols - 1) / cols;
  int16_t gridTop = 30;
  int16_t gridBottom = h - 36;
  int16_t cellW = w / cols;
  int16_t cellH = (gridBottom - gridTop) / rows;

  tft.setTextFont(1);
  tft.setTextSize(2);
  for (size_t i = 0; i < BAUD_RATE_OPTIONS_COUNT; i++) {
    int col = i % cols;
    int row = i / cols;
    Rect r = { (int16_t)(col * cellW + 3), (int16_t)(gridTop + row * cellH + 3),
               (int16_t)(cellW - 6), (int16_t)(cellH - 6) };
    optionRect_[i] = r;

    bool selected = (BAUD_RATE_OPTIONS[i] == currentBaud);
    uint16_t bg = selected ? COLOR_OVERLAY_BTN_SEL : COLOR_OVERLAY_BTN;
    tft.fillRect(r.x, r.y, r.w, r.h, bg);
    tft.drawRect(r.x, r.y, r.w, r.h, TFT_BLACK);
    tft.setTextColor(COLOR_BTN_TEXT, bg);
    tft.drawString(String(BAUD_RATE_OPTIONS[i]), r.x + r.w / 2, r.y + r.h / 2);
  }

  cancelRect_ = { 10, (int16_t)(h - 30), (int16_t)(w - 20), 24 };
  tft.fillRect(cancelRect_.x, cancelRect_.y, cancelRect_.w, cancelRect_.h, COLOR_BTN_BG_ACTIVE);
  tft.drawRect(cancelRect_.x, cancelRect_.y, cancelRect_.w, cancelRect_.h, TFT_BLACK);
  tft.setTextFont(2);
  tft.setTextSize(1);
  tft.setTextColor(COLOR_BTN_TEXT, COLOR_BTN_BG_ACTIVE);
  tft.drawString("Cancel", w / 2, cancelRect_.y + cancelRect_.h / 2);

  tft.setTextDatum(TL_DATUM);
}

uint32_t BaudMenu::hitTest(int16_t x, int16_t y) const {
  for (size_t i = 0; i < BAUD_RATE_OPTIONS_COUNT; i++) {
    if (optionRect_[i].contains(x, y)) return BAUD_RATE_OPTIONS[i];
  }
  if (cancelRect_.contains(x, y)) return BAUD_MENU_CANCEL;
  return 0;
}
