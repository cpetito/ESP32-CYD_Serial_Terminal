// TermBuffer.h
// Stores word-wrapped display lines in a fixed-capacity ring buffer and
// tracks the current scroll position for the on-screen history view.

#pragma once

#include <Arduino.h>
#include "Config.h"

class TermBuffer {
public:
  TermBuffer();

  // Formats a millis() timestamp as "[mm:ss.mmm]" (rolls over after ~99 min,
  // which is fine for a live monitoring session).
  static String formatTimestamp(unsigned long ms);

  // Takes one completed raw line (already stripped of its terminator),
  // stamps it, word-wraps it to `colsPerLine`, and appends the resulting
  // display line(s) to the ring buffer.
  void addLine(const String &rawLine, unsigned long timestampMs, uint16_t colsPerLine);

  void clear();

  // Total number of word-wrapped display lines currently stored.
  uint16_t count() const { return count_; }

  // Returns the display line at logical index [0 = oldest .. count()-1 = newest].
  const String &lineAt(uint16_t index) const;

  // Scroll offset in lines, measured back from the newest line (0 = pinned
  // to bottom / most recent).
  uint16_t scrollOffset() const { return scrollOffset_; }
  void setScrollOffset(int32_t offset);
  void scrollBy(int32_t deltaLines);
  bool isAtBottom() const { return scrollOffset_ == 0; }
  uint16_t maxScrollOffset(uint16_t visibleRows) const;

private:
  String lines_[HISTORY_CAPACITY];
  uint16_t head_;     // index of the oldest line
  uint16_t count_;    // number of valid lines
  uint16_t scrollOffset_;

  void pushWrapped(const String &text);
};
