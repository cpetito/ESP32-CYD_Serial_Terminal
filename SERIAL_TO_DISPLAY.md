# Serial-in-to-display pipeline

A technical reference for how a byte arriving on GPIO35 ends up as pixels
on the TFT. Written for whoever next needs to touch this code without
re-deriving it from scratch.

## Overview

```
GPIO35 (RX) ──▶ SerialCapture::poll()  ──▶ onLineReceived()  ──▶ TermBuffer::addLine()  ──▶ DisplayUI::render()
   (UART2)         assembles raw lines      (.ino callback)      timestamp, word-wrap,        draws visible
                    from bytes, non-               │              store in ring buffer          slice to TFT
                    blocking                        ▼
                                              SDLogger::writeLine()
                                              (only while recording)
```

Every stage is driven from `loop()`, once per iteration, with no
blocking calls anywhere in the chain:

```cpp
void loop() {
  capture.poll();   // 1. drain UART bytes, assemble lines
  pollTouch();       //    (touch handling - not part of this pipeline)
  display.render(...); // 4. redraw, throttled + dirty-flag gated
}
```

## 1. Capturing bytes: `SerialCapture` (`SerialCapture.h/.cpp`)

- `MONITOR_UART` (`Serial2`) is opened RX-only: `begin(baud, SERIAL_8N1, MONITOR_RX_PIN /* 35 */, -1)`.
  Passing `-1` for TX means this UART only ever reads; GPIO35 is
  input-only anyway, so this is also a hardware constraint, not just a
  choice.
- `poll()` drains up to `kMaxBytesPerPoll` (512) bytes per call via
  `available()`/`read()` — bounded so a fast/noisy source can't starve
  touch handling and rendering in the same `loop()` iteration.
- A small state machine assembles raw lines byte by byte:
  - `haveLineStart_`/`lineStartMs_` capture `millis()` at the *first*
    non-terminator byte of a line — the timestamp reflects when the line
    **started** arriving, not when it finished.
  - `\r` ends the line and sets `sawCR_`, so an immediately-following `\n`
    is swallowed as the second half of a CR/LF pair rather than producing
    a second, empty line.
  - `\n` ends the line unless `sawCR_` is set (per above).
  - Anything else is appended to `lineBuffer_`.
  - `MAX_RAW_LINE_LEN` (512 chars) forces a line break even with no
    terminator, so a stuck/garbage source can't grow `lineBuffer_`
    unbounded.
- On completion, it calls the registered callback —
  `callback_(lineBuffer_, timestampMs, ctx_)` — synchronously, from
  inside `poll()`. There is no queue: everything downstream in this
  pipeline runs on the same call stack, once per completed line, within
  the same `loop()` iteration that read the last byte of it.
- `setBaudRate()` tears down and restarts the UART, discarding any
  in-progress partial line (bytes captured at two different bit rates
  would just be garbage mixed together).

## 2. The callback: `onLineReceived()` (`.ino`)

Registered once in `setup()`:
```cpp
capture.setLineCallback(onLineReceived, nullptr);
```

Runs once per completed raw line:
```cpp
static void onLineReceived(const String &line, unsigned long timestampMs, void *ctx) {
  history.addLine(line, timestampMs, display.colsPerLine());
  if (recording) {
    sdLogger.writeLine(TermBuffer::formatTimestamp(timestampMs) + " " + line);
  }
  display.markTerminalDirty();
}
```
Three things happen, in order: the line is timestamped/wrapped/stored
(§3), optionally logged to the SD card verbatim with the same timestamp
format, and the terminal is flagged for redraw. `display.colsPerLine()`
is passed in on every call rather than cached, so word-wrap always
matches the display's current geometry.

## 3. Timestamping, word-wrap, storage: `TermBuffer` (`TermBuffer.h/.cpp`)

This is the model layer — it owns the scrollback and knows nothing about
SPI or pixels.

- `formatTimestamp(ms)` renders `millis()` as `[mm:ss.mmm]` (wraps at 100
  minutes — fine for a live monitoring session, not meant for multi-hour
  unattended logs).
- `addLine(rawLine, timestampMs, colsPerLine)`:
  - Builds the timestamp prefix, plus an equal-width blank `indent`
    string so word-wrapped continuation lines align under the text
    rather than repeating the timestamp.
  - Word-wraps to `colsPerLine`, preferring to break at the last space
    before the limit; falls back to a hard break mid-token if there's no
    space to use (e.g. one very long unbroken string).
  - A blank raw line still produces one wrapped line (just the
    timestamp), so gaps in traffic stay visible rather than vanishing.
  - Each wrapped line is pushed individually via `pushWrapped()` — one
    raw line can produce several stored lines.
- `pushWrapped(text)`:
  - Fixed-capacity ring buffer (`lines_[HISTORY_CAPACITY]`, 400 entries).
    Once full, the oldest line is overwritten (`head_` advances).
  - Also owns the **autoscroll** state (`autoscroll_`,
    `isAutoscrollOn()`/`setAutoscroll()`) and reconciles it against
    `scrollOffset_` on every single push: pinned to `0` (the bottom)
    while autoscrolling, otherwise incremented (and clamped) so paused
    content stays in view rather than drifting — this has to run even
    when the offset was already `0`, since "paused right at the bottom"
    and "still following" otherwise look identical. See the AUTO/PAUSED
    bugfix in git history for why this matters.
- Read-side API the display layer uses: `count()`, `lineAt(index)`
  (oldest = `0`), `scrollOffset()`, `maxScrollOffset(visibleRows)`,
  `isAtBottom()`.

## 4. Rendering: `DisplayUI` (`DisplayUI.h/.cpp`)

- `colsPerLine()`/`visibleRows()` are computed once in `begin()` from the
  TFT's actual resolution and the fixed GLCD font metrics (`CHAR_W=6`,
  `CHAR_H=8`, `TERM_TEXT_SIZE=1`), so `TermBuffer`'s word-wrap always
  matches what physically fits.
- `render(buf, baudRate, recStatus, recording, autoscroll)` is called
  from `loop()`, throttled to at most once per `NEW_DATA_FLUSH_MS` (10ms).
  It only actually redraws the status bar and/or terminal area if their
  respective dirty flags are set (`markStatusDirty()`/
  `markTerminalDirty()`) — avoiding SPI traffic on iterations where
  nothing changed.
- `drawTerminal(buf)`: using `buf.scrollOffset()` and `visibleRows()`,
  computes which slice of the ring buffer is currently visible
  (bottom-aligning when there isn't enough history to fill the screen),
  then draws each visible line with `tft.print()`, colorizing the leading
  `[mm:ss.mmm]` timestamp differently from the payload text.

`DisplayUI` has no opinion of its own about autoscroll or scroll
position — it just draws whatever `TermBuffer::scrollOffset()` currently
is. All the scroll/pause logic lives in `TermBuffer` and the touch
handling in the `.ino` (drag gestures and the AUTO/PAUSED button both
call `history.scrollBy()`/`setAutoscroll()`, never touch `DisplayUI`
directly).

## Key files at a glance

| File | Owns |
|---|---|
| `SerialCapture.{h,cpp}` | Byte-level UART reading, line assembly, arrival timestamp |
| `TermBuffer.{h,cpp}` | Timestamp formatting, word-wrap, ring buffer, scroll/autoscroll state |
| `DisplayUI.{h,cpp}` | Reading `TermBuffer` and drawing the visible slice to the TFT |
| `SDLogger.{h,cpp}` | Optional verbatim copy of each line to the SD card |
| `ESP32_CYD_Serial_Terminal.ino` | Wires the above together; owns touch-driven scroll/pause input |

## Tunables (`Config.h`)

| Constant | Meaning |
|---|---|
| `MONITOR_RX_PIN` / `MONITOR_UART` | GPIO35 / `Serial2` — the monitored line |
| `MAX_RAW_LINE_LEN` (512) | Forces a line break if no CR/LF ever arrives |
| `HISTORY_CAPACITY` (400) | Ring buffer size, in word-wrapped display lines |
| `NEW_DATA_FLUSH_MS` (10) | Minimum gap between `display.render()` calls |
| `CHAR_W`/`CHAR_H`/`TERM_TEXT_SIZE` | Font metrics driving `colsPerLine()`/`visibleRows()` |
