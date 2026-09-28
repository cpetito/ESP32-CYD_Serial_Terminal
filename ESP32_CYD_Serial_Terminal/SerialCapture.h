// SerialCapture.h
// Non-blocking reader (and simple writer) for the monitored UART. Call
// poll() from loop() on every pass; it drains whatever bytes are
// currently available without ever blocking, accumulates a raw line, and
// reports completed lines via a callback. CR, LF and CR/LF are all
// treated as line terminators, with CR/LF collapsed into a single
// terminator.

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

  // Writes text followed by TX_LINE_ENDING out MONITOR_TX_PIN. A short
  // canned message (a handful of characters) at any of this sketch's
  // supported baud rates transmits well within one UART FIFO's worth of
  // buffering, so this doesn't need to be broken up across poll() calls
  // the way receiving is - it's fire-and-forget from the caller's side.
  void sendLine(const char *text);

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
