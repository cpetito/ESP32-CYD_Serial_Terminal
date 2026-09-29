# SEND (canned-message TX) flow

A technical reference for how tapping the SEND button transmits a
canned message out GPIO27 to the monitored device's RX pin. Companion
to [BAUD_SELECT.md](BAUD_SELECT.md) (same overlay-menu shape) and
[SERIAL_TO_DISPLAY.md](SERIAL_TO_DISPLAY.md) (the RX side of the same
UART); written for whoever next needs to touch this code without
re-deriving it from scratch.

## Overview

```
tap SEND button ──▶ TxMenu::draw()  (mode = MODE_TX_MENU)
                          │
                    tap a message / Cancel / miss
                          │
                          ▼
                handleTxMenuTap() ──▶ SerialCapture::sendLine()
                          │              (MONITOR_UART.print(text + "\r\n"))
                          ▼
                mode = MODE_TERMINAL, full redraw
```

Like the baud menu, this is **not** something that runs every `loop()`
iteration — it's a one-shot, user-initiated overlay that temporarily
takes over rendering. While `mode == MODE_TX_MENU`, `loop()`'s render
gate skips `display.render()` entirely; the overlay is drawn once on
entry and only redrawn (by drawing the terminal again) on selection,
cancel, or a miss.

## 1. Opening the menu

`handleTerminalTap()` (`.ino`), on a tap inside `display.btnSend()`'s
rect:
```cpp
if (display.btnSend().contains(x, y)) {
  mode = MODE_TX_MENU;
  txMenu.draw(display.tft());
  return;
}
```
`mode` is the same plain global used by the baud menu
(`enum AppMode { MODE_TERMINAL, MODE_BAUD_MENU, MODE_TX_MENU };`), so
it gates both which tap handler `pollTouch()` dispatches to (§3 below)
and whether `loop()` calls `display.render()` at all — exactly the
mechanism [TOUCH_INPUT.md](TOUCH_INPUT.md) and
[BAUD_SELECT.md](BAUD_SELECT.md) already describe for `MODE_BAUD_MENU`,
just with a third value. Unlike `BaudMenu::draw()`, `TxMenu::draw()`
takes no "currently selected" argument — there's no persistent notion
of a currently-active canned message the way there is for baud rate,
so nothing needs pre-highlighting.

## 2. Laying out and hit-testing: `TxMenu` (`TxMenu.h/.cpp`)

Deliberately a near-copy of `BaudMenu`'s pattern rather than a shared
base class — two overlay menus didn't justify the abstraction, and
keeping them independent means a future change to one (e.g. more rows,
different button colors) doesn't risk the other.

- `draw()` computes a 2-column grid sized to fit `TX_MESSAGES_COUNT`
  entries (`Config.h`: currently `RESET` and `STATUS`, so one row) plus
  a Cancel bar along the bottom, storing each button's `Rect` in
  `optionRect_[]` as it draws them — same rectangles `hitTest()` checks
  later, so layout and hit-testing can't drift apart (see `UITypes.h`'s
  `Rect`, shared with the status bar buttons and `BaudMenu`).
- Unlike `BaudMenu`, there's no `COLOR_OVERLAY_BTN_SEL` highlight logic
  — every button is drawn the same way, since there's no "current"
  message to distinguish.
- `hitTest(x, y)` returns the tapped message's index into
  `TX_MESSAGES` (`0`, `1`, ...), or `-1` for a miss *or* an explicit tap
  on Cancel:
  ```cpp
  int TxMenu::hitTest(int16_t x, int16_t y) const {
    for (size_t i = 0; i < TX_MESSAGES_COUNT; i++) {
      if (optionRect_[i].contains(x, y)) return (int)i;
    }
    return -1; // miss or Cancel - caller treats both the same
  }
  ```
  `BaudMenu` needs a separate `BAUD_MENU_CANCEL` sentinel distinct from
  `0` because `0` is not a valid baud rate but *is* a valid array
  index; here every valid result is already `>= 0`, so a single `-1`
  unambiguously means "nothing to send."

## 3. Dispatch: `handleTxMenuTap()` (`.ino`)

Called from `pollTouch()` on a tap release while `mode == MODE_TX_MENU`.
`pollTouch()`'s release branch is a three-way dispatch keyed on `mode`:
```cpp
} else if (!touched && touchWasDown) {
    if (!touchDragged) {
      if (mode == MODE_TERMINAL) {
        handleTerminalTap(touchDownX, touchDownY);
      } else if (mode == MODE_BAUD_MENU) {
        handleBaudMenuTap(touchDownX, touchDownY);
      } else {
        handleTxMenuTap(touchDownX, touchDownY);
      }
    }
  }
```
and `handleTxMenuTap()` itself:
```cpp
static void handleTxMenuTap(int16_t x, int16_t y) {
  int idx = txMenu.hitTest(x, y);
  if (idx >= 0) {
    capture.sendLine(TX_MESSAGES[idx].text);
  }
  // A miss and an explicit Cancel both just close the menu - see
  // TxMenu::hitTest()'s comment for why there's no separate sentinel.
  mode = MODE_TERMINAL;
  display.markStatusDirty();
  display.markTerminalDirty();
}
```
Both outcomes (send or don't) fall through to the same three lines —
the menu always closes on release, whether or not anything was
transmitted. As with the baud menu, both dirty flags are set on the
way out: the overlay painted over the entire display, so the status
bar and terminal both need a full repaint once control returns to
`MODE_TERMINAL`.

## 4. Transmitting: `SerialCapture::sendLine()` (`SerialCapture.cpp`)

```cpp
void SerialCapture::sendLine(const char *text) {
  MONITOR_UART.print(text);
  MONITOR_UART.print(TX_LINE_ENDING);
  Serial.printf("TX: %s%s", text, TX_LINE_ENDING);
}
```
Writes straight to `MONITOR_UART` (the same `Serial2` instance §1 of
[SERIAL_TO_DISPLAY.md](SERIAL_TO_DISPLAY.md) reads from) — no queue or
state machine, unlike receiving. A canned message is a handful of
characters; at every baud rate this sketch offers, that's well within
one UART FIFO's worth of hardware buffering, so `print()` returns
almost immediately and there's nothing to poll for completion the way
`SerialCapture::poll()` has to drain bytes incrementally on the RX
side. The third line mirrors whatever was sent to the USB Serial
Monitor (115200 baud) as `TX: RESET\r\n`, purely so a message can be
confirmed sent without needing a second device wired to GPIO27 (a
single-device round-trip test — looping GPIO27 back into GPIO22 with a
jumper wire — is what actually confirmed this feature end-to-end;
whatever was sent shows up on screen as a received line right after,
same as it would from a real external device).

## 5. Why GPIO27, not GPIO35

`MONITOR_TX_PIN` is GPIO27, not GPIO35, even though GPIO35 sits on the
same P3 connector as `MONITOR_RX_PIN` (GPIO22) and was the first choice
considered. GPIO35 is one of the ESP32's four input-only pins
(34/35/36/39): unlike the internal-pull-up situation on
`MONITOR_RX_PIN` (see [SERIAL_TO_DISPLAY.md](SERIAL_TO_DISPLAY.md) /
`Config.h`), this isn't a missing pull resistor — these four pins have
**no output driver at the silicon level at all**. Configuring one as
TX wouldn't error or misbehave subtly; `MONITOR_UART.begin()` would
silently succeed and every `sendLine()` call would just transmit
nothing. GPIO27 is the other pin confirmed present and unused on this
board's P3 connector, so it's the one actually wired to
`MONITOR_UART`'s TX line:
```cpp
MONITOR_UART.begin(baudRate_, SERIAL_8N1, MONITOR_RX_PIN, MONITOR_TX_PIN);
```

## Key files at a glance

| File | Owns |
|---|---|
| `TxMenu.{h,cpp}` | Overlay layout, hit-testing |
| `SerialCapture.{h,cpp}` | `sendLine()` — the actual UART write |
| `Config.h` | `MONITOR_TX_PIN`, `TX_MESSAGES[]`, `TX_LINE_ENDING` |
| `ESP32_CYD_Serial_Terminal.ino` | `mode` state, open/dispatch glue |

## Tunables

| Constant | Where | Meaning |
|---|---|---|
| `TX_MESSAGES[]` | `Config.h` | Label/text pairs offered on the SEND menu — add, remove, or edit entries and re-flash |
| `TX_LINE_ENDING` ("\r\n") | `Config.h` | Appended to every message; same ending for all of them, no per-message override |
| `MONITOR_TX_PIN` (27) | `Config.h` | GPIO driving TX — see §5 for why it can't be GPIO35 |
