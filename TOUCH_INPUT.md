# Touch input flow

A technical reference for how a finger on the glass becomes a scroll,
a tap, or a button press. Companion to
[SERIAL_TO_DISPLAY.md](SERIAL_TO_DISPLAY.md); written for whoever next
needs to touch this code without re-deriving it from scratch.

## Overview

```
XPT2046 (IRQ + SoftSPI) ──▶ TouchInput::getPoint() ──▶ pollTouch() ──▶ handleTerminalTap() / handleBaudMenuTap()
   raw ADC read,                mapped to screen         press/drag/tap        button dispatch, or
   screen-coordinate                coordinates          state machine         history.scrollBy()/setAutoscroll()
   mapping
```

Like the serial pipeline, this runs once per `loop()` iteration with no
blocking calls:

```cpp
void loop() {
  capture.poll();     // (not part of this pipeline)
  pollTouch();         // 2-3. read touch, run the gesture state machine
  display.render(...); //      (dispatch happens inside pollTouch, above)
}
```

## 1. Reading the panel: `TouchInput` (`TouchInput.h/.cpp`)

- The XPT2046 is driven over **SoftSPI** (bit-banged, no hardware SPI
  peripheral) rather than a `SPIClass(HSPI/VSPI)` instance. This is
  deliberate: the ESP32 classic has only two hardware SPI peripherals,
  and this board needs three independent buses (TFT, touch, microSD -
  see `SDLogger.h`/`Config.h` for the full story of the SD/TFT conflict
  that motivated it). Touch is the lowest-bandwidth of the three, so it's
  the one that gives up its hardware peripheral.
- `SoftSPI`/`XPT2046_TouchscreenSOFTSPI` are vendored unmodified from
  RandomNerdTutorials' CYD example rather than hand-rolled — the exact
  XPT2046 bit timing is a hardware detail that's easy to get subtly
  wrong without a scope to verify against (an earlier hand-rolled
  attempt in this project's history did get it wrong).
- `getPoint(x, y)`:
  - `tirqTouched()` is a cheap check of a flag set by a GPIO interrupt on
    the XPT2046's IRQ pin — no SPI traffic. `touched()` is what actually
    issues SPI traffic and checks pressure against a threshold. Checking
    both, in that order, means idle polling (nothing pressed) costs
    almost nothing.
  - `touchscreen_.setRotation(1)` is set once in `begin()` — this
    specific library's *passthrough* rotation mode, meaning the raw
    values it returns are the true, unprocessed ADC reading rather than
    something the library has already rotated/mirrored internally. This
    matters because the mapping below was calibrated against raw
    hardware-SPI-library values; passthrough keeps that calibration
    valid regardless of which touch backend is underneath.
  - Raw-to-screen mapping (`Config.h`): `TOUCH_SWAP_XY`/`TOUCH_INVERT_X`/
    `TOUCH_INVERT_Y` select which raw axis feeds screen X vs Y and which
    direction each runs, and `TOUCH_RAW_X/Y_MIN/MAX` are the calibration
    range. On this board, confirmed by testing, no swap or inversion is
    needed (`TOUCH_SWAP_XY 0`, both inverts `0`) — the flags exist for a
    different panel/revision, not because this one needs them. Setting
    `TOUCH_DEBUG_SERIAL` to `1` prints raw and mapped coordinates for
    re-deriving these on a different panel — see
    [TOUCH_CALIBRATION.md](TOUCH_CALIBRATION.md) for the full model,
    the corner-tap procedure, and a case study on a symptom that looked
    like a calibration problem but wasn't.
  - Result is `constrain()`ed to `[0, screenW_-1] x [0, screenH_-1]`
    before returning.

## 2. The gesture state machine: `pollTouch()` (`.ino`)

Called every `loop()` iteration. Tracks a small amount of state across
calls (`touchWasDown`, `touchDownX/Y`, `touchLastY`, `touchDownMs`,
`dragAccumPx`, `touchDragged`) to turn a stream of `getPoint()` samples
into press/drag/release events:

- **Press** (`touched && !touchWasDown`): records the down position and
  time, resets the per-gesture accumulators.
- **Held** (`touched && touchWasDown`): computes `dy` since the last
  sample. Only in `MODE_TERMINAL` (not while the baud menu is open) does
  it consider this a drag, and only once **both**:
  - `DRAG_MIN_HOLD_MS` (60ms) have elapsed since the press, **and**
  - cumulative movement has exceeded `DRAG_THRESHOLD_PX` (12px) —

  Both guards exist for the same reason: a resistive panel's first
  reading or two after contact is often noisy while pressure settles, so
  a plain tap can easily jitter past a naive threshold and get
  misclassified as a drag (which would silently swallow the tap). Once
  `touchDragged` is set for this gesture, later samples skip re-checking
  the threshold (`touchDragged ||` short-circuits it) — a real drag
  doesn't need to re-clear the bar on every sample.

  While dragging, pixel movement accumulates in `dragAccumPx` and is
  converted to whole display lines (`CHAR_H * TERM_TEXT_SIZE` px/line)
  via `history.scrollBy()`, with the fractional remainder kept for the
  next sample rather than lost. Dragging down (positive `dy`) scrolls
  toward older content — `scrollBy()`'s offset is "distance back from
  the newest line," so this is the natural pull-down-to-see-history
  direction. After each scroll step, `clampScroll()` keeps the offset
  within `history.maxScrollOffset()`, and `history.setAutoscroll(history.isAtBottom())`
  updates the single source of truth for the AUTO/PAUSED state (see
  [SERIAL_TO_DISPLAY.md](SERIAL_TO_DISPLAY.md) for why `TermBuffer`, not
  a separate bool here, owns that flag). See
  [DRAG_SCROLL.md](DRAG_SCROLL.md) for the full pixel-accumulator math,
  the bounds-clamping split between `TermBuffer` and the `.ino`, and how
  the result gets rendered.
- **Release** (`!touched && touchWasDown`): if the gesture never crossed
  the drag threshold, it's a tap — dispatched to `handleTerminalTap()` or
  `handleBaudMenuTap()` depending on `mode`, using the **press** position
  (`touchDownX/Y`), not wherever the finger happened to lift off.

## 3. Dispatch: tap handlers (`.ino`)

- `handleTerminalTap(x, y)` — active in `MODE_TERMINAL`. Hit-tests the
  four status-bar buttons (`display.btnBaud()/btnRec()/btnClear()/
  btnAutoscroll()`, each a `Rect` computed once in `DisplayUI::begin()`)
  in order, first match wins:
  - **Baud** → switches to `MODE_BAUD_MENU` and draws the overlay
    (`BaudMenu::draw()`).
  - **REC** → `toggleRecording()` (starts/stops `SDLogger` session
    recording — see `SERIAL_TO_DISPLAY.md`'s SD-logging note).
  - **CLR** → `history.clear()` (also resets autoscroll to on).
  - **AUTO/PAUSED** → `history.setAutoscroll(!history.isAutoscrollOn())`.
  - A tap inside the terminal area itself (not on any button) is a no-op
    here — drags over that same area are handled separately, above.
- `handleBaudMenuTap(x, y)` — active in `MODE_BAUD_MENU`. Delegates the
  hit-test to `baudMenu.hitTest(x, y)`, which returns either a baud rate,
  `BAUD_MENU_CANCEL`, or `0` (missed every button — treated the same as
  cancel). On a real selection: `settings.setBaudRate()` (persists to
  Preferences/NVS) and `capture.setBaudRate()` (re-inits the monitored
  UART at the new speed) both run, then the mode switches back to
  `MODE_TERMINAL` and both dirty flags are set so the full terminal view
  redraws over the now-stale overlay.

## Why touch owns none of the scroll/render state directly

Both the drag handler and the AUTO button call into `TermBuffer`
(`history.scrollBy()`/`setAutoscroll()`/`clear()`) rather than keeping
their own copy of scroll position or the autoscroll flag. `DisplayUI`
then just draws whatever `TermBuffer` currently reports. This one-way
flow (touch → `TermBuffer` → `DisplayUI`, never touch → `DisplayUI`
directly, and never two places tracking the same flag) is what the
AUTO/PAUSED bugfix (see git history) put in place, after an earlier
version kept a separate `autoscroll` bool in the `.ino` that could — and
did — drift out of sync with `TermBuffer`'s own scroll offset.

## Key files at a glance

| File | Owns |
|---|---|
| `TouchInput.{h,cpp}` | Raw XPT2046 read, screen-coordinate mapping |
| `SoftSPI.{h,cpp}` | Vendored bit-banged SPI transport (RandomNerdTutorials) |
| `XPT2046_TouchscreenSOFTSPI.{h,cpp}` | Vendored XPT2046 protocol driver (RandomNerdTutorials) |
| `BaudMenu.{h,cpp}` | Baud-select overlay layout + hit-testing |
| `UITypes.h` | `Rect`, the shared hit-test type for both the status bar and the baud menu |
| `ESP32_CYD_Serial_Terminal.ino` | `pollTouch()` gesture state machine, tap dispatch |

## Tunables

| Constant | Where | Meaning |
|---|---|---|
| `TOUCH_CLK/MOSI/MISO/CS/IRQ_PIN` | `Config.h` | XPT2046 wiring |
| `TOUCH_RAW_X/Y_MIN/MAX` | `Config.h` | Calibration range for the raw-to-screen `map()` |
| `TOUCH_SWAP_XY`/`TOUCH_INVERT_X`/`TOUCH_INVERT_Y` | `Config.h` | Axis orientation (unneeded — `0`/`0`/`0` — on this board) |
| `TOUCH_DEBUG_SERIAL` | `Config.h` | Prints raw+mapped coordinates per touch, for re-calibrating |
| `DRAG_THRESHOLD_PX` (12) / `DRAG_MIN_HOLD_MS` (60) | `.ino` | Tap-vs-drag debounce against resistive-panel jitter |
