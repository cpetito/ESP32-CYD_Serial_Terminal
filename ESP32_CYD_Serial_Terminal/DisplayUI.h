// DisplayUI.h
// Owns the TFT_eSPI instance and draws the status bar + scrolling terminal
// view, redrawing only the regions marked dirty.

#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>
#include "Config.h"
#include "TermBuffer.h"
#include "UITypes.h"

class DisplayUI {
public:
  void begin();

  // Layout, computed once in begin() from the panel's actual resolution so
  // it keeps working if rotation/size constants change.
  uint16_t colsPerLine() const { return cols_; }
  uint16_t visibleRows() const { return rows_; }

  // Status bar button hit-boxes, shared with TouchUI.
  const Rect &btnBaud() const { return btnBaud_; }
  const Rect &btnClear() const { return btnClear_; }
  const Rect &btnRec() const { return btnRec_; }
  const Rect &btnAutoscroll() const { return btnAutoscroll_; }
  const Rect &terminalArea() const { return terminalArea_; }

  void markTerminalDirty() { terminalDirty_ = true; }
  void markStatusDirty() { statusDirty_ = true; }

  // Redraws whatever is marked dirty. Cheap to call every loop() pass.
  void render(TermBuffer &buf, uint32_t baudRate, const String &recStatus,
              bool recording, bool autoscroll);

  TFT_eSPI &tft() { return tft_; }

private:
  TFT_eSPI tft_ = TFT_eSPI();
  uint16_t cols_ = 0;
  uint16_t rows_ = 0;

  Rect terminalArea_;
  Rect btnBaud_, btnClear_, btnRec_, btnAutoscroll_;

  bool terminalDirty_ = true;
  bool statusDirty_ = true;

  void drawStatusBar(uint32_t baudRate, const String &recStatus, bool recording, bool autoscroll);
  void drawTerminal(TermBuffer &buf);
};
