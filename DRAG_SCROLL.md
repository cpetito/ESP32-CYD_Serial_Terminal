# Drag-scroll flow

A technical reference for how dragging a finger across the terminal
area turns into scrolled history, and how that scroll position turns
back into pixels. Companion to [TOUCH_INPUT.md](TOUCH_INPUT.md) (which
covers tap-vs-drag classification, upstream of everything here) and
[SERIAL_TO_DISPLAY.md](SERIAL_TO_DISPLAY.md) (the ring buffer this
scrolls through); written for whoever next needs to touch this code
without re-deriving it from scratch.

## Overview

```
drag sample (dy) ──▶ pixel accumulator ──▶ whole lines ──▶ TermBuffer::scrollBy()
                                                                   │
                                                            clampScroll()
                                                                   │
                                                    TermBuffer::setAutoscroll(isAtBottom())
                                                                   │
                                                            markTerminalDirty()
                                                                   │
                                              DisplayUI::drawTerminal() picks the visible slice
```

This is the "held, and classified as a drag" branch of `pollTouch()`'s
gesture state machine — see [TOUCH_INPUT.md](TOUCH_INPUT.md) §2 for how
a touch gets to this point (the `DRAG_THRESHOLD_PX`/`DRAG_MIN_HOLD_MS`
debounce) rather than being treated as a tap instead. This doc picks up
from there: what happens, per sample, once a gesture *is* a drag.

## 1. Pixels to lines: the accumulator (`pollTouch()`, `.ino`)

```cpp
dragAccumPx += dy;
int16_t lineHeightPx = CHAR_H * TERM_TEXT_SIZE;
int32_t lines = dragAccumPx / lineHeightPx;
if (lines != 0) {
  history.scrollBy(lines);
  clampScroll();
  history.setAutoscroll(history.isAtBottom());
  display.markTerminalDirty();
  display.markStatusDirty();
  dragAccumPx -= lines * lineHeightPx;
}
```
`dy` (this sample's vertical movement since the last one) is added to a
running total, `dragAccumPx`, rather than converted to lines and
discarded on every single sample. A touch panel is sampled far more
often than one line-height of movement typically happens in a single
sample, so converting `dy` directly to lines every time would either
round to zero constantly (nothing visibly scrolls) or, with naive
rounding, drop or double-count fractional pixels across samples. Instead:
only the **whole-line** portion of the accumulator is consumed each time
(`lines = dragAccumPx / lineHeightPx`, integer division), and only that
consumed portion is subtracted back out (`dragAccumPx -= lines *
lineHeightPx`) — the fractional remainder carries forward to accumulate
with the *next* sample's `dy`, so no sub-line movement is ever lost, just
deferred until it adds up to a whole line.

`lineHeightPx` is `CHAR_H * TERM_TEXT_SIZE` — the exact pixel height of
one rendered line (see [SERIAL_TO_DISPLAY.md](SERIAL_TO_DISPLAY.md) §4),
so one full line of drag distance always corresponds to exactly one line
of scroll, regardless of font size changes to those constants.

## 2. Direction and bounds: `TermBuffer::scrollBy()`/`clampScroll()`

`history.scrollBy(lines)` is called with a **positive** `lines` value
when the drag moved down (`dy > 0`), and that scrolls **toward older**
content:
```cpp
void TermBuffer::scrollBy(int32_t deltaLines) {
  int32_t newOffset = (int32_t)scrollOffset_ + deltaLines;
  if (newOffset < 0) newOffset = 0;
  scrollOffset_ = (uint16_t)newOffset;
}
```
This lines up with `scrollOffset_`'s own definition — "distance back
from the newest line" (see
[SERIAL_TO_DISPLAY.md](SERIAL_TO_DISPLAY.md) §3) — so *increasing* the
offset moves further into the past, and dragging down to reveal earlier
content (the same gesture as scrolling up a page, or pulling down to see
chat history) is the natural mapping, not an arbitrary sign choice.

`scrollBy()` only clamps the **lower** bound (never negative). The upper
bound — don't scroll back further than there's actual history to show —
is enforced separately, immediately after, by the `.ino`'s own
`clampScroll()`:
```cpp
static void clampScroll() {
  uint16_t maxOff = history.maxScrollOffset(display.visibleRows());
  if (history.scrollOffset() > maxOff) history.setScrollOffset(maxOff);
}
```
`maxScrollOffset(visibleRows)` (`TermBuffer.cpp`) returns `0` if there's
not even enough history to fill the screen (nothing to scroll to), or
`count() - visibleRows` otherwise — the offset at which the *oldest*
line is at the top of the screen and there's nothing further back to
reveal. This two-step split (unclamped math in `scrollBy()`, clamping
against display geometry in the `.ino`) exists because `TermBuffer`
doesn't know how many rows are currently visible on screen —
that's `DisplayUI`'s concern — so the buffer can't clamp the upper bound
itself without being handed that number on every call.

## 3. Autoscroll hookup

Immediately after scrolling: `history.setAutoscroll(history.isAtBottom())`.
This is the *only* place a drag touches the autoscroll flag, and it
always sets it to whatever `isAtBottom()` (`scrollOffset_ == 0`)
currently evaluates to — never unconditionally on or off. Two
consequences:
- Dragging away from the bottom (offset becomes `> 0`) always disables
  autoscroll — the same effect as tapping AUTO to pause, just via a
  different gesture.
- Dragging all the way back down to the bottom re-enables it — so a drag
  is also how you can resume auto-follow, not just the AUTO/PAUSED
  button.

See [TOUCH_INPUT.md](TOUCH_INPUT.md)'s note on why touch code always
goes through `TermBuffer` for this rather than keeping a separate flag —
this call is a direct instance of that principle.

## 4. Rendering the result: `DisplayUI::drawTerminal()`

Nothing drag-specific happens at render time — this is the exact same
code path any redraw uses (new data arriving, `CLR`, `AUTO`/`PAUSED`, or
a drag), reading whatever `TermBuffer::scrollOffset()` currently is:
```cpp
int32_t lastIndex = (int32_t)total - 1 - (int32_t)buf.scrollOffset();
int32_t firstIndex = lastIndex - (int32_t)rows_ + 1;
if (firstIndex < 0) firstIndex = 0;
int32_t numLines = lastIndex - firstIndex + 1;
int32_t startRow = rows_ - numLines; // bottom-align when history is short
```
`lastIndex` is the newest visible line (index `count()-1` when at the
bottom, smaller the further back `scrollOffset()` is); `firstIndex` is
`rows_` lines further back, clamped to `0` (the oldest line ever stored)
rather than going negative. `startRow` bottom-aligns a short history —
if there's less content than screen rows (e.g. right after `CLR`, or
early in a session), the drawn lines start lower on screen rather than
at the very top, leaving blank space above rather than below, matching
how a normal terminal fills from the bottom up.

## A subtlety: new data arriving mid-drag

Because everything in this app runs single-threaded, one `loop()`
iteration at a time (see [SERIAL_TO_DISPLAY.md](SERIAL_TO_DISPLAY.md)'s
overview), `capture.poll()` — which can call `TermBuffer::addLine()` and
thus `pushWrapped()`'s own autoscroll/offset bookkeeping — always runs
*before* `pollTouch()` within the same iteration. So within any single
iteration, an incoming line is fully processed (including whatever
`pushWrapped()` decides about the current offset) before that
iteration's touch sample is read. There's no genuine race — but across
*iterations*, if new data arrives in between two samples of an
in-progress drag, and autoscroll happens to still read as "on" at that
exact moment (e.g. very early in a drag, before `setAutoscroll(false)`
has run yet this gesture), that line's arrival will snap the view back
to the bottom, and the in-progress drag's accumulated offset from
`scrollBy()` calls so far is overwritten. This is a narrow window (only
matters in the first sample or two of a drag that started while still
autoscrolling) and hasn't been reported as a practical problem, but it's
a real, understood interaction, not an oversight — worth knowing if a
"drag felt like it jumped" report ever comes in.

## Key files at a glance

| File | Owns |
|---|---|
| `ESP32_CYD_Serial_Terminal.ino` | Pixel-to-line accumulation, `clampScroll()`, autoscroll hookup |
| `TermBuffer.cpp` | `scrollBy()`/`setScrollOffset()`/`maxScrollOffset()`/`setAutoscroll()` |
| `DisplayUI.cpp` | Turning the current offset back into a visible slice of lines |

## Tunables

| Constant | Where | Meaning |
|---|---|---|
| `CHAR_H`/`TERM_TEXT_SIZE` | `Config.h` | Pixel height of one line — the accumulator's conversion unit |
| `DRAG_THRESHOLD_PX`/`DRAG_MIN_HOLD_MS` | `.ino` | Upstream tap-vs-drag classification — see [TOUCH_INPUT.md](TOUCH_INPUT.md) |
