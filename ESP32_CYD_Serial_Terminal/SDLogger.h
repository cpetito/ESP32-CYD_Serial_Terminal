// SDLogger.h
// Optional session recording to a microSD card. Each recording session
// writes to a freshly numbered file under /sessions.

#pragma once

#include <Arduino.h>
#include <SPI.h>
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

  // Root-level write + read-back, independent of session/directory logic -
  // isolates whether the card can be written to at all vs. a path-specific
  // problem. Logs its own PASS/FAIL detail (prefixed with `label`) to
  // Serial. Public so it can be re-run on demand (e.g. right after other
  // peripherals init, to check whether *their* setup disturbs the SD bus)
  // rather than only once, automatically, inside mount().
  void runWriteSelfTest(const char *label = "SD");

private:
  // A dedicated hardware SPI peripheral, exclusively the SD card's own -
  // not the global default `SPI` object, which TFT_eSPI already owns. See
  // TouchInput.h for why this board needs each of its three SPI devices
  // on its own bus (two get a hardware peripheral each; touch is
  // bit-banged) rather than reusing one peripheral with swapped pins.
  SPIClass sdSPI_ = SPIClass(HSPI);

  bool mounted_ = false;
  File file_;
  String currentFileName_;
  String logDir_; // "" means the card's root - see mount()'s mkdir fallback
  unsigned long lastFlushMs_ = 0;
};
