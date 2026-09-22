// SerialCapture.cpp
#include "SerialCapture.h"

void SerialCapture::begin(uint32_t baudRate) {
  baudRate_ = baudRate;
  // RX-only: pass -1 for the TX pin since we never transmit on this UART.
  MONITOR_UART.begin(baudRate_, SERIAL_8N1, MONITOR_RX_PIN, -1);
  lineBuffer_ = "";
  haveLineStart_ = false;
  sawCR_ = false;
}

void SerialCapture::setBaudRate(uint32_t baudRate) {
  if (baudRate == baudRate_) return;
  MONITOR_UART.end();
  // Flush any partial line rather than mixing bytes captured at two speeds.
  lineBuffer_ = "";
  haveLineStart_ = false;
  sawCR_ = false;
  begin(baudRate);
}

void SerialCapture::finishLine() {
  if (callback_) {
    unsigned long ts = haveLineStart_ ? lineStartMs_ : millis();
    callback_(lineBuffer_, ts, ctx_);
  }
  lineBuffer_ = "";
  haveLineStart_ = false;
}

void SerialCapture::poll() {
  // Bound the number of bytes drained per call so an extremely fast/noisy
  // source can't starve the display and touch handling in loop().
  const int kMaxBytesPerPoll = 512;
  int processed = 0;

  while (MONITOR_UART.available() && processed < kMaxBytesPerPoll) {
    int b = MONITOR_UART.read();
    if (b < 0) break;
    processed++;
    char c = (char)b;

    if (!haveLineStart_ && c != '\r' && c != '\n') {
      lineStartMs_ = millis();
      haveLineStart_ = true;
    }

    if (c == '\r') {
      finishLine();
      sawCR_ = true;
      continue;
    }
    if (c == '\n') {
      if (sawCR_) {
        // Second half of a CR/LF pair - already terminated on the CR.
        sawCR_ = false;
      } else {
        finishLine();
      }
      continue;
    }

    sawCR_ = false;
    if (lineBuffer_.length() < MAX_RAW_LINE_LEN) {
      lineBuffer_ += c;
    } else if (lineBuffer_.length() == MAX_RAW_LINE_LEN) {
      // Force a break so a line with no terminator doesn't grow forever.
      finishLine();
      lineStartMs_ = millis();
      haveLineStart_ = true;
      lineBuffer_ += c;
    }
  }
}
