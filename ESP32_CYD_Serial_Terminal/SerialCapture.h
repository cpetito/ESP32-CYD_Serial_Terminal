// SerialCapture.h
// Non-blocking reader for the monitored UART. Call poll() from loop() on
// every pass; it drains whatever bytes are currently available without
// ever blocking, accumulates a raw line, and reports completed lines via
// a callback. CR, LF and CR/LF are all treated as line terminators, with
// CR/LF collapsed into a single terminator.

#pragma once

#include <Arduino.h>
#include "Config.h"

class SerialCapture {
public:
  using LineCallback = void (*)(const String &line, unsigned long timestampMs, void *ctx);

  void begin(uint32_t baudRate);
  void setBaudRate(uint32_t baudRate); // re-inits the UART at a new speed
  uint32_t baudRate() const { return baudRate_; }

  void setLineCallback(LineCallback cb, void *ctx) {
    callback_ = cb;
    ctx_ = ctx;
  }

  // Drains all currently-available bytes without blocking. Safe to call
  // every loop() iteration.
  void poll();

private:
  uint32_t baudRate_ = DEFAULT_BAUD_RATE;
  String lineBuffer_;
  unsigned long lineStartMs_ = 0;
  bool haveLineStart_ = false;
  bool sawCR_ = false;

  LineCallback callback_ = nullptr;
  void *ctx_ = nullptr;

  void finishLine();
};
