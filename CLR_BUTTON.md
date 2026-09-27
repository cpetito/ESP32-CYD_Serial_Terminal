# CLR button flow

A technical reference for what tapping CLR actually does. Companion to
[SERIAL_TO_DISPLAY.md](SERIAL_TO_DISPLAY.md),
[TOUCH_INPUT.md](TOUCH_INPUT.md), [BAUD_SELECT.md](BAUD_SELECT.md) and
[SD_LOGGING.md](SD_LOGGING.md); written for whoever next needs to touch
this code without re-deriving it from scratch. This is the simplest of
the five flows — one button, one call, no persistence, no overlay — so
this doc is short on purpose.

## Overview

```
tap CLR ──▶ handleTerminalTap() ──▶ TermBuffer::clear() ──▶ mark both dirty ──▶ next render() draws an empty screen
```

## 1. Dispatch: `handleTerminalTap()` (`.ino`)

```cpp
if (display.btnClear().contains(x, y)) {
  history.clear(); // also resets to autoscroll-on
  display.markTerminalDirty();
  display.markStatusDirty();
  return;
}
```
Same tap-handling path as every other status-bar button — see
[TOUCH_INPUT.md](TOUCH_INPUT.md) for how a touch release becomes a call
into this function with the original press coordinates. Both dirty
flags are set because clearing changes what both halves of the screen
should show: the terminal area (now empty) and the status bar (the
AUTO/PAUSED button's label, since `clear()` also resets autoscroll to
on — see below).

## 2. The reset itself: `TermBuffer::clear()` (`TermBuffer.cpp`)

```cpp
void TermBuffer::clear() {
  head_ = 0;
  count_ = 0;
  scrollOffset_ = 0;
  autoscroll_ = true;
  for (uint16_t i = 0; i < HISTORY_CAPACITY; i++) {
    lines_[i] = "";
  }
}
```
Resets the ring buffer to empty (`head_`/`count_` back to zero — the old
`String` contents are also explicitly cleared rather than just letting
`count_ == 0` hide them, freeing their heap allocations immediately
rather than waiting for new lines to overwrite each slot), and resets
**both** scroll-related fields: `scrollOffset_` to `0` and, deliberately,
`autoscroll_` to `true` regardless of whatever it was set to before.

That second reset is a UX choice, not an implementation necessity: if
you'd paused the view (see [TOUCH_INPUT.md](TOUCH_INPUT.md)'s AUTO/PAUSED
note) and then hit CLR, there is no longer any old content to have been
paused *at* — resuming auto-follow for whatever comes in next is the
only sensible state to land in. This is also why CLR needs
`markStatusDirty()` in addition to `markTerminalDirty()`: the
AUTO/PAUSED button's label can silently flip from PAUSED back to AUTO as
a side effect of clearing, and the status bar needs to be told to
repaint to reflect that.

## 3. Next redraw: `DisplayUI::drawTerminal()`

Nothing CLR-specific happens here — it's the same render path every
other line-received redraw uses (see
[SERIAL_TO_DISPLAY.md](SERIAL_TO_DISPLAY.md) §4), just with
`buf.count() == 0`:
```cpp
uint16_t total = buf.count();
if (total == 0) return;
```
`drawTerminal()` has already filled the terminal area with the
background color before this check, so an empty buffer just means the
function does nothing further — no lines to compute a visible slice of,
no special "empty state" rendering needed.

## What CLR does *not* touch

- **Recording**: an in-progress SD recording (see
  [SD_LOGGING.md](SD_LOGGING.md)) is unaffected — `recording` and the
  open file handle live entirely in `SDLogger`/the `.ino`'s `recording`
  flag, not in `TermBuffer`. Clearing the on-screen history does not
  clear, truncate, or otherwise touch whatever's already been written to
  the card.
- **Baud rate**: no interaction with `Settings`/`SerialCapture` at all
  (contrast with [BAUD_SELECT.md](BAUD_SELECT.md), which does persist
  and re-init).
- **Incoming data mid-clear**: `TermBuffer::clear()` and
  `SerialCapture::poll()`/`onLineReceived()` never run concurrently —
  everything in this app is single-threaded and sequential within one
  `loop()` iteration (see [SERIAL_TO_DISPLAY.md](SERIAL_TO_DISPLAY.md)'s
  overview) — so there's no window where a line could be added between
  the buffer being cleared and the redraw.

## Key files at a glance

| File | Owns |
|---|---|
| `TermBuffer.{h,cpp}` | The actual clear operation and its autoscroll-reset side effect |
| `ESP32_CYD_Serial_Terminal.ino` | CLR tap dispatch, marking both dirty flags |
