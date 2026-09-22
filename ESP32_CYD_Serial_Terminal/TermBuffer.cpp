// TermBuffer.cpp
#include "TermBuffer.h"

TermBuffer::TermBuffer()
  : head_(0), count_(0), scrollOffset_(0) {
}

String TermBuffer::formatTimestamp(unsigned long ms) {
  unsigned long totalSeconds = ms / 1000UL;
  unsigned int millisPart = ms % 1000UL;
  unsigned int minutesPart = (totalSeconds / 60UL) % 100UL; // wrap at 100 min for a fixed-width field
  unsigned int secondsPart = totalSeconds % 60UL;

  char buf[16];
  snprintf(buf, sizeof(buf), "[%02u:%02u.%03u]", minutesPart, secondsPart, millisPart);
  return String(buf);
}

void TermBuffer::clear() {
  head_ = 0;
  count_ = 0;
  scrollOffset_ = 0;
  for (uint16_t i = 0; i < HISTORY_CAPACITY; i++) {
    lines_[i] = "";
  }
}

void TermBuffer::pushWrapped(const String &text) {
  uint16_t writeIndex;
  if (count_ < HISTORY_CAPACITY) {
    writeIndex = (head_ + count_) % HISTORY_CAPACITY;
    count_++;
  } else {
    // Buffer full: overwrite the oldest line and advance head.
    writeIndex = head_;
    head_ = (head_ + 1) % HISTORY_CAPACITY;
  }
  lines_[writeIndex] = text;

  // A new line always lands at the "newest" end, which increases every
  // older line's distance from the bottom by one. If the user has scrolled
  // back (offset > 0), bump the offset to keep the same content in view
  // instead of letting it silently drift toward the bottom.
  if (scrollOffset_ > 0) {
    scrollOffset_++;
    uint16_t maxOffset = (count_ > 0) ? (count_ - 1) : 0;
    if (scrollOffset_ > maxOffset) scrollOffset_ = maxOffset;
  }
}

void TermBuffer::addLine(const String &rawLine, unsigned long timestampMs, uint16_t colsPerLine) {
  if (colsPerLine < 4) colsPerLine = 4; // sanity floor

  String prefix = formatTimestamp(timestampMs) + " ";
  String indent;
  for (uint16_t i = 0; i < prefix.length(); i++) indent += ' ';

  String content = rawLine;
  bool firstSegment = true;

  if (content.length() == 0) {
    // Blank line: still show the timestamp so gaps in traffic are visible.
    pushWrapped(prefix);
    return;
  }

  while (content.length() > 0) {
    const String &lead = firstSegment ? prefix : indent;
    uint16_t avail = (colsPerLine > lead.length()) ? (colsPerLine - lead.length()) : 1;

    if (content.length() <= avail) {
      pushWrapped(lead + content);
      content = "";
      break;
    }

    // Prefer to break at the last space within the available width so we
    // don't split words mid-token.
    int breakAt = -1;
    for (int i = (int)avail; i > 0; i--) {
      if (content.charAt(i) == ' ') { breakAt = i; break; }
    }

    if (breakAt <= 0) {
      // No good space to break on (or a single very long token) - hard wrap.
      breakAt = avail;
      pushWrapped(lead + content.substring(0, breakAt));
      content = content.substring(breakAt);
    } else {
      pushWrapped(lead + content.substring(0, breakAt));
      content = content.substring(breakAt + 1); // skip the space
    }

    firstSegment = false;
  }
}

const String &TermBuffer::lineAt(uint16_t index) const {
  static const String empty = "";
  if (index >= count_) return empty;
  uint16_t actual = (head_ + index) % HISTORY_CAPACITY;
  return lines_[actual];
}

uint16_t TermBuffer::maxScrollOffset(uint16_t visibleRows) const {
  if (count_ <= visibleRows) return 0;
  return count_ - visibleRows;
}

void TermBuffer::setScrollOffset(int32_t offset) {
  if (offset < 0) offset = 0;
  scrollOffset_ = (uint16_t)offset;
}

void TermBuffer::scrollBy(int32_t deltaLines) {
  int32_t newOffset = (int32_t)scrollOffset_ + deltaLines;
  if (newOffset < 0) newOffset = 0;
  scrollOffset_ = (uint16_t)newOffset;
}
