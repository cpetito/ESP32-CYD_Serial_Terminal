// DisplayUI.cpp
#include "DisplayUI.h"

void DisplayUI::begin() {
  tft_.init();
  tft_.setRotation(SCREEN_ROTATION);
  tft_.fillScreen(COLOR_BG);

  pinMode(TFT_BL_PIN, OUTPUT);
  digitalWrite(TFT_BL_PIN, TFT_BACKLIGHT_ON);

  int16_t w = tft_.width();
  int16_t h = tft_.height();

  terminalArea_ = { 0, STATUS_BAR_HEIGHT, w, (int16_t)(h - STATUS_BAR_HEIGHT) };
  cols_ = terminalArea_.w / (CHAR_W * TERM_TEXT_SIZE);
  rows_ = terminalArea_.h / (CHAR_H * TERM_TEXT_SIZE);

  // Four equal-ish status bar buttons across the top.
  int16_t bw = w / 4;
  btnBaud_       = { 0,            0, bw, STATUS_BAR_HEIGHT };
  btnRec_        = { (int16_t)(bw * 1), 0, bw, STATUS_BAR_HEIGHT };
  btnClear_      = { (int16_t)(bw * 2), 0, bw, STATUS_BAR_HEIGHT };
  btnAutoscroll_ = { (int16_t)(bw * 3), 0, (int16_t)(w - bw * 3), STATUS_BAR_HEIGHT };

  terminalDirty_ = true;
  statusDirty_ = true;
}

static void drawButtonLabel(TFT_eSPI &tft, const Rect &r, const String &label,
                             uint16_t bg, uint16_t fg) {
  tft.fillRect(r.x, r.y, r.w, r.h, bg);
  tft.drawRect(r.x, r.y, r.w, r.h, TFT_BLACK);
  tft.setTextColor(fg, bg);
  tft.setTextFont(1);
  tft.setTextSize(1);
  tft.setTextDatum(MC_DATUM);
  tft.drawString(label, r.x + r.w / 2, r.y + r.h / 2);
  tft.setTextDatum(TL_DATUM);
}

void DisplayUI::drawStatusBar(uint32_t baudRate, const String &recStatus, bool recording, bool autoscroll) {
  drawButtonLabel(tft_, btnBaud_, String(baudRate), COLOR_BTN_BG, COLOR_BTN_TEXT);
  drawButtonLabel(tft_, btnRec_, recStatus, recording ? COLOR_BTN_BG_ACTIVE : COLOR_BTN_BG, COLOR_BTN_TEXT);
  drawButtonLabel(tft_, btnClear_, "CLR", COLOR_BTN_BG, COLOR_BTN_TEXT);
  drawButtonLabel(tft_, btnAutoscroll_, autoscroll ? "AUTO" : "PAUSED", COLOR_BTN_BG, COLOR_BTN_TEXT);
}

void DisplayUI::drawTerminal(TermBuffer &buf) {
  tft_.fillRect(terminalArea_.x, terminalArea_.y, terminalArea_.w, terminalArea_.h, COLOR_BG);

  uint16_t total = buf.count();
  if (total == 0) return;

  int32_t lastIndex = (int32_t)total - 1 - (int32_t)buf.scrollOffset();
  if (lastIndex < 0) return;
  int32_t firstIndex = lastIndex - (int32_t)rows_ + 1;
  if (firstIndex < 0) firstIndex = 0;

  int32_t numLines = lastIndex - firstIndex + 1;
  int32_t startRow = rows_ - numLines; // bottom-align when history is short

  tft_.setTextFont(1);
  tft_.setTextSize(TERM_TEXT_SIZE);
  tft_.setTextDatum(TL_DATUM);

  for (int32_t i = 0; i < numLines; i++) {
    uint16_t idx = (uint16_t)(firstIndex + i);
    int16_t rowY = terminalArea_.y + (int16_t)((startRow + i) * CHAR_H * TERM_TEXT_SIZE);
    const String &line = buf.lineAt(idx);

    // Colorize the leading "[mm:ss.mmm] " timestamp differently from the
    // payload text, when present, purely for readability.
    int closeBracket = -1;
    int searchLimit = min((int)line.length(), 13);
    for (int c = 0; c < searchLimit; c++) {
      if (line.charAt(c) == ']') { closeBracket = c; break; }
    }

    int16_t cx = terminalArea_.x;
    if (closeBracket >= 0) {
      String tsPart = line.substring(0, closeBracket + 1);
      String rest = line.substring(closeBracket + 1);
      tft_.setTextColor(COLOR_TIMESTAMP, COLOR_BG);
      tft_.setCursor(cx, rowY);
      tft_.print(tsPart);
      cx += tsPart.length() * CHAR_W * TERM_TEXT_SIZE;
      tft_.setTextColor(COLOR_TEXT, COLOR_BG);
      tft_.setCursor(cx, rowY);
      tft_.print(rest);
    } else {
      tft_.setTextColor(COLOR_TEXT, COLOR_BG);
      tft_.setCursor(cx, rowY);
      tft_.print(line);
    }
  }
}

void DisplayUI::render(TermBuffer &buf, uint32_t baudRate, const String &recStatus,
                        bool recording, bool autoscroll) {
  if (statusDirty_) {
    drawStatusBar(baudRate, recStatus, recording, autoscroll);
    statusDirty_ = false;
  }
  if (terminalDirty_) {
    drawTerminal(buf);
    terminalDirty_ = false;
  }
}
