# Baud-select flow

A technical reference for how tapping the baud button changes what
speed the monitored UART listens at, and how that choice survives a
reboot. Companion to [SERIAL_TO_DISPLAY.md](SERIAL_TO_DISPLAY.md) and
[TOUCH_INPUT.md](TOUCH_INPUT.md); written for whoever next needs to
touch this code without re-deriving it from scratch.

## Overview

```
tap "115200" button ──▶ BaudMenu::draw()  (mode = MODE_BAUD_MENU)
                              │
                         tap a rate / Cancel
                              │
                              ▼
                    handleBaudMenuTap() ──▶ Settings::setBaudRate()   (persist to NVS)
                              │        └──▶ SerialCapture::setBaudRate() (re-init UART2)
                              ▼
                    mode = MODE_TERMINAL, full redraw
```

Unlike the serial and touch pipelines, this one is **not** something that
runs every `loop()` iteration — it's a one-shot, user-initiated overlay
that temporarily takes over rendering. See `loop()`'s comment: while
`mode == MODE_BAUD_MENU`, `display.render()` is not called at all; the
overlay is drawn once on entry and only redrawn on selection/cancel.

## 1. Opening the menu

`handleTerminalTap()` (`.ino`), on a tap inside `display.btnBaud()`'s
rect:
```cpp
mode = MODE_BAUD_MENU;
baudMenu.draw(display.tft(), capture.baudRate());
```
`mode` is a plain global (`enum AppMode { MODE_TERMINAL, MODE_BAUD_MENU }`)
that gates both which tap handler `pollTouch()` dispatches to (see
[TOUCH_INPUT.md](TOUCH_INPUT.md)) and whether `loop()` calls
`display.render()` at all. `BaudMenu::draw()` takes the TFT directly
(not through `DisplayUI`) and the *currently active* baud rate
(`capture.baudRate()`, not `settings.getBaudRate()` — these are normally
the same value, but `capture`'s live rate is the more honest source of
truth for what to highlight) so the matching button can be drawn
pre-selected.

## 2. Laying out and hit-testing: `BaudMenu` (`BaudMenu.h/.cpp`)

- `draw()` computes a 3-column grid sized to fit all of
  `BAUD_RATE_OPTIONS` (`Config.h`: 1200 through 921600, 12 options) plus
  a Cancel bar along the bottom, and stores each button's `Rect` in
  `optionRect_[]`/`cancelRect_` as it draws them — the exact same
  rectangles are what `hitTest()` checks later, so layout and hit-testing
  can never drift out of sync with each other (see `UITypes.h`'s `Rect`,
  shared with the status bar buttons).
- The button matching `currentBaud` is filled with
  `COLOR_OVERLAY_BTN_SEL` instead of the normal `COLOR_OVERLAY_BTN`, so
  the active rate is visually obvious without needing separate label
  text.
- `hitTest(x, y)` returns the tapped baud rate, `BAUD_MENU_CANCEL`
  (`0xFFFFFFFF`) for the Cancel bar, or `0` if the tap missed every
  button (e.g. a stray tap in the gutter between buttons). `0` and
  `BAUD_MENU_CANCEL` are handled identically by the caller (below) —
  missing every button and explicitly cancelling both just close the
  menu with no change.

## 3. Dispatch: `handleBaudMenuTap()` (`.ino`)

Called from `pollTouch()` on a tap release while `mode == MODE_BAUD_MENU`
(see [TOUCH_INPUT.md](TOUCH_INPUT.md) for how taps vs. drags are told
apart upstream of this):

```cpp
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
```

Both branches mark the whole screen dirty on the way out — the overlay
just painted over the entire display, so both the status bar and
terminal area need a full repaint once control returns to
`MODE_TERMINAL`, not just whatever normally changes per `loop()` tick.

On an actual selection, two independent things happen, deliberately in
this order:
1. `settings.setBaudRate(sel)` — persists to NVS immediately (see §4),
   so the choice survives a power cycle even if something else goes
   wrong right after.
2. `capture.setBaudRate(sel)` — takes effect immediately, live (see §5).

## 4. Persistence: `Settings` (`Settings.h/.cpp`)

A thin wrapper over the ESP32 `Preferences` library (NVS-backed
key/value storage):
- `begin()` (called once in `setup()`) opens the `cydterm` namespace and
  loads the last-saved baud rate, defaulting to `DEFAULT_BAUD_RATE`
  (115200) if none was ever saved.
- `setBaudRate()` updates both the in-RAM cached value
  (`getBaudRate()` is just a field read, not a fresh NVS query) and the
  persisted one, in that order within a single call — there's no
  intermediate state where they could disagree.
- `nextSessionNumber()` lives in the same class since it's the same
  underlying storage mechanism, but is otherwise unrelated to baud rate
  — see [SD_LOGGING.md](SD_LOGGING.md).

## 5. Applying it live: `SerialCapture::setBaudRate()` (`SerialCapture.cpp`)

```cpp
void SerialCapture::setBaudRate(uint32_t baudRate) {
  if (baudRate == baudRate_) return;
  MONITOR_UART.end();
  lineBuffer_ = "";
  haveLineStart_ = false;
  sawCR_ = false;
  begin(baudRate);
}
```
Guards against a no-op re-init if the same rate is somehow selected
again, then tears down and restarts the UART. Critically, it also
discards any partially-assembled line (`lineBuffer_`) rather than
letting it survive the speed change — bytes captured before and after a
baud change are not the same signal, and stitching them into one "line"
would just produce garbage. Any single line that happens to be
mid-capture at the exact moment the rate changes is silently lost; this
is an accepted, unavoidable cost of changing the listening speed live
rather than something worth working around.

## Key files at a glance

| File | Owns |
|---|---|
| `BaudMenu.{h,cpp}` | Overlay layout, hit-testing |
| `Settings.{h,cpp}` | NVS persistence of the selected baud rate |
| `SerialCapture.{h,cpp}` | Applying a new baud rate to the live UART |
| `ESP32_CYD_Serial_Terminal.ino` | `mode` state, open/dispatch glue |

## Tunables

| Constant | Where | Meaning |
|---|---|---|
| `BAUD_RATE_OPTIONS[]` | `Config.h` | The 12 rates offered in the grid |
| `DEFAULT_BAUD_RATE` (115200) | `Config.h` | Used until anything is ever saved |
| `PREFS_NAMESPACE`/`PREFS_KEY_BAUD` | `Config.h` | NVS namespace/key `Settings` uses |
