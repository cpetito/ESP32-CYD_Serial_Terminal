// SDLogger.h
// Optional session recording to a microSD card. Each recording session
// writes to a freshly numbered file under /sessions.

#pragma once

#include <Arduino.h>
#include <SD.h>
#include "Config.h"
#include "Settings.h"

class SDLogger {
public:
  // Attempts to mount the card. Safe to call repeatedly (e.g. each time the
  // user opens the recording toggle) in case a card was inserted later.
  bool mount();
  bool isMounted() const { return mounted_; }

  // Starts a new session file (e.g. /sessions/session_0007.log). Returns
  // false (and leaves recording off) if the card isn't mounted or the file
  // can't be opened.
  bool startSession(Settings &settings, String &outFileName);
  void stopSession();
  bool isRecording() const { return file_; }

  // Appends one already-formatted line (with trailing newline) to the open
  // session file. Flushes periodically rather than on every write to reduce
  // flash wear and I/O stalls.
  void writeLine(const String &line);

  const String &currentFileName() const { return currentFileName_; }

  // Human-readable status for the UI ("no card", "ready", "REC").
  String statusText() const;

private:
  bool mounted_ = false;
  File file_;
  String currentFileName_;
  String logDir_; // "" means the card's root - see mount()'s mkdir fallback
  unsigned long lastFlushMs_ = 0;

  // Root-level write + read-back, independent of session/directory logic -
  // isolates whether the card can be written to at all vs. a path-specific
  // problem. Logs its own PASS/FAIL detail to Serial.
  void runWriteSelfTest();
};
