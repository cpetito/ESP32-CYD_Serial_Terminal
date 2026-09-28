// TxMenu.cpp
#include "TxMenu.h"

void TxMenu::draw(TFT_eSPI &tft) {
  int16_t w = tft.width();
  int16_t h = tft.height();

  tft.fillScreen(COLOR_OVERLAY_BG);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(COLOR_STATUS_TEXT, COLOR_OVERLAY_BG);
  tft.setTextFont(2);
  tft.setTextSize(1);
  tft.drawString("Send Message", w / 2, 16);

  const int cols = 2;
  const int rows = (TX_MESSAGES_COUNT + cols - 1) / cols;
  int16_t gridTop = 30;
  int16_t gridBottom = h - 36;
  int16_t cellW = w / cols;
  int16_t cellH = (gridBottom - gridTop) / rows;

  tft.setTextFont(1);
  tft.setTextSize(2);
  for (size_t i = 0; i < TX_MESSAGES_COUNT; i++) {
    int col = i % cols;
    int row = i / cols;
    Rect r = { (int16_t)(col * cellW + 3), (int16_t)(gridTop + row * cellH + 3),
               (int16_t)(cellW - 6), (int16_t)(cellH - 6) };
    optionRect_[i] = r;

    tft.fillRect(r.x, r.y, r.w, r.h, COLOR_OVERLAY_BTN);
    tft.drawRect(r.x, r.y, r.w, r.h, TFT_BLACK);
    tft.setTextColor(COLOR_BTN_TEXT, COLOR_OVERLAY_BTN);
    tft.drawString(TX_MESSAGES[i].label, r.x + r.w / 2, r.y + r.h / 2);
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

int TxMenu::hitTest(int16_t x, int16_t y) const {
  for (size_t i = 0; i < TX_MESSAGES_COUNT; i++) {
    if (optionRect_[i].contains(x, y)) return (int)i;
  }
  return -1; // miss or Cancel - caller treats both the same
}
