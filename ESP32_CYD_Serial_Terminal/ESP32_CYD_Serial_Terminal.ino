// ESP32_CYD_Serial_Terminal.ino
//
// Receive-only serial monitor / logger for the ESP32-2432S028R "Cheap
// Yellow Display" board (2.8" ILI9341 + XPT2046 touch).
//
// - Listens on GPIO35 (RX only) for another device's TX line.
// - Non-blocking capture; CR, LF or CR/LF all terminate a line.
// - Each line is timestamped with millis() and word-wrapped on screen.
// - Scrollable history via touch drag; touch buttons for baud selection,
//   clearing the screen and pausing/resuming auto-scroll.
// - Baud rate is touch-selectable from a menu and persisted in Preferences.
// - Optional session recording to a microSD card, one fresh file per
//   recording session.
//
// See README.md for wiring notes and the required TFT_eSPI User_Setup.h.

#include "Config.h"
#include "TermBuffer.h"
#include "SerialCapture.h"
#include "Settings.h"
#include "SDLogger.h"
#include "DisplayUI.h"
#include "TouchInput.h"
#include "BaudMenu.h"

enum AppMode { MODE_TERMINAL, MODE_BAUD_MENU };

static Settings settings;
static DisplayUI display;
static TermBuffer history;
static SerialCapture capture;
static TouchInput touchInput;
static SDLogger sdLogger;
static BaudMenu baudMenu;

static AppMode mode = MODE_TERMINAL;
static bool recording = false;

// --- Touch gesture state (shared press/drag/tap tracking) -----------------
static bool touchWasDown = false;
static int16_t touchDownX = 0, touchDownY = 0;
static int16_t touchLastY = 0;
static unsigned long touchDownMs = 0;
static int32_t dragAccumPx = 0;
static bool touchDragged = false;

// A resistive panel's very first reading or two after finger contact is
// often noisy while pressure settles, so a tap can easily jitter past a
// tiny pixel threshold and get misread as a drag (suppressing the tap).
// Requiring a bit of both distance AND elapsed hold time before committing
// to "this is a drag" filters that out without blunting real swipes, which
// move much further than this over a much longer hold.
static const int16_t DRAG_THRESHOLD_PX = 12;
static const unsigned long DRAG_MIN_HOLD_MS = 60;

static void toggleRecording() {
  if (recording) {
    sdLogger.stopSession();
    recording = false;
  } else {
    String fileName;
    if (sdLogger.startSession(settings, fileName)) {
      recording = true;
    }
    // If it failed (no card, etc.), sdLogger.statusText() will report why
    // via the REC button label - no separate popup needed.
  }
  display.markStatusDirty();
}

static void clampScroll() {
  uint16_t maxOff = history.maxScrollOffset(display.visibleRows());
  if (history.scrollOffset() > maxOff) history.setScrollOffset(maxOff);
}

static void handleTerminalTap(int16_t x, int16_t y) {
  if (display.btnBaud().contains(x, y)) {
    mode = MODE_BAUD_MENU;
    baudMenu.draw(display.tft(), capture.baudRate());
    return;
  }
  if (display.btnRec().contains(x, y)) {
    toggleRecording();
    return;
  }
  if (display.btnClear().contains(x, y)) {
    history.clear(); // also resets to autoscroll-on
    display.markTerminalDirty();
    display.markStatusDirty();
    return;
  }
  if (display.btnAutoscroll().contains(x, y)) {
    history.setAutoscroll(!history.isAutoscrollOn());
    display.markTerminalDirty();
    display.markStatusDirty();
    return;
  }
  // Tap inside the terminal area itself: no action (drag is handled
  // separately below).
}

static void handleBaudMenuTap(int16_t x, int16_t y) {
  uint32_t sel = baudMenu.hitTest(x, y);
  if (sel == BAUD_MENU_CANCEL || sel == 0) {
    mode = MODE_TERMINAL;
    display.markStatusDirty();
    display.markTerminalDirty();
    return;
  }
  settings.setBaudRate(sel);
  capture.setBaudRate(sel);
  mode = MODE_TERMINAL;
  display.markStatusDirty();
  display.markTerminalDirty();
}

static void pollTouch() {
  int16_t tx, ty;
  bool touched = touchInput.getPoint(tx, ty);

  if (touched && !touchWasDown) {
    touchDownX = tx;
    touchDownY = ty;
    touchLastY = ty;
    touchDownMs = millis();
    dragAccumPx = 0;
    touchDragged = false;
  } else if (touched && touchWasDown) {
    int16_t dy = ty - touchLastY;
    touchLastY = ty;

    bool heldLongEnough = (millis() - touchDownMs) >= DRAG_MIN_HOLD_MS;
    if (mode == MODE_TERMINAL && (touchDragged ||
        (heldLongEnough && abs(ty - touchDownY) >= DRAG_THRESHOLD_PX))) {
      touchDragged = true;
      dragAccumPx += dy;
      int16_t lineHeightPx = CHAR_H * TERM_TEXT_SIZE;
      int32_t lines = dragAccumPx / lineHeightPx;
      if (lines != 0) {
        history.scrollBy(lines); // drag down -> positive dy -> scroll toward older
        clampScroll();
        history.setAutoscroll(history.isAtBottom());
        display.markTerminalDirty();
        display.markStatusDirty();
        dragAccumPx -= lines * lineHeightPx;
      }
    }
  } else if (!touched && touchWasDown) {
    if (!touchDragged) {
      if (mode == MODE_TERMINAL) {
        handleTerminalTap(touchDownX, touchDownY);
      } else {
        handleBaudMenuTap(touchDownX, touchDownY);
      }
    }
  }

  touchWasDown = touched;
}

static void onLineReceived(const String &line, unsigned long timestampMs, void *ctx) {
  (void)ctx;
  history.addLine(line, timestampMs, display.colsPerLine());

  if (recording) {
    String stamped = TermBuffer::formatTimestamp(timestampMs) + " " + line;
    sdLogger.writeLine(stamped);
  }

  // Pinning to the bottom (or not) while autoscrolling is paused is
  // handled inside TermBuffer::addLine() itself now - see pushWrapped().
  display.markTerminalDirty();
}

void setup() {
  Serial.begin(115200); // USB CDC, for the sketch's own debug output only

  settings.begin();

  // Mounted here, before the TFT/touch peripherals run, so the REC button
  // shows accurate SD status from the very first frame rather than only
  // after the first tap.
  sdLogger.mount();

  display.begin();
  touchInput.begin(display.tft().width(), display.tft().height());

  capture.setLineCallback(onLineReceived, nullptr);
  capture.begin(settings.getBaudRate());

  display.markStatusDirty();
  display.markTerminalDirty();
}

void loop() {
  capture.poll();
  pollTouch();

  if (mode == MODE_TERMINAL) {
    static unsigned long lastRenderMs = 0;
    unsigned long now = millis();
    if (now - lastRenderMs >= NEW_DATA_FLUSH_MS) {
      display.render(history, capture.baudRate(), sdLogger.statusText(), recording, history.isAutoscrollOn());
      lastRenderMs = now;
    }
  }
  // While MODE_BAUD_MENU is active, the overlay is drawn once (on entry)
  // and only re-drawn on selection/cancel, so no per-loop render is needed.
}
