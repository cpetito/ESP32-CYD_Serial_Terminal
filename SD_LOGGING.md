# SD-logging flow

A technical reference for how tapping REC turns into a growing file on
the microSD card, and how a fresh file gets picked for each recording.
Companion to [SERIAL_TO_DISPLAY.md](SERIAL_TO_DISPLAY.md),
[TOUCH_INPUT.md](TOUCH_INPUT.md) and
[BAUD_SELECT.md](BAUD_SELECT.md); written for whoever next needs to
touch this code without re-deriving it from scratch. This one has the
longest, most hard-won debugging history in the project — see the
"Why VSPI, not HSPI" and "read-only card" notes below before assuming a
future SD problem is a new bug.

## Overview

```
setup() ──▶ SDLogger::mount()  (re-verified on every call, not cached)
                    │
              tap REC (not recording) ──▶ toggleRecording() ──▶ SDLogger::startSession()
                    │                                                  │
                    │                                          mount() again (fresh check)
                    │                                                  │
                    │                                    Settings::nextSessionNumber() (persisted)
                    │                                                  │
                    │                                     open /sessions/session_NNNN.log
                    │
              onLineReceived() (every line, while recording) ──▶ SDLogger::writeLine()
                    │
              tap REC (recording) ──▶ toggleRecording() ──▶ SDLogger::stopSession()
```

Like baud selection, this is user-initiated rather than a per-`loop()`
pipeline — but unlike baud selection, one part of it (`writeLine()`)
*does* run once per received line for as long as a recording is active,
tying it directly into the serial pipeline (see
[SERIAL_TO_DISPLAY.md](SERIAL_TO_DISPLAY.md) §2).

## 1. Mounting: `SDLogger::mount()` (`SDLogger.cpp`)

Called once at the top of `setup()` (so the REC button shows accurate
status from the first frame) and again every time `startSession()` runs
(i.e. every REC tap that starts a new recording):

- **Deliberately re-verifies the card on every call rather than caching
  success.** An earlier version short-circuited if a previous mount had
  succeeded, so `statusText()` kept reporting `SD OK` indefinitely after
  the card was physically removed — nothing ever re-checked. `mount()`
  is only ever called at boot and once per REC tap, never in a hot loop,
  so a full `SD.end()` + `SPI.begin()` + `SD.begin()` re-check on every
  call is cheap enough to always do for real.
- Uses a **dedicated hardware SPI peripheral**, `sdSPI_` = `SPIClass(VSPI)`
  — not the Arduino-global `SPI` object, and specifically not `HSPI`.
  This board's required `TFT_eSPI` `User_Setup.h` defines
  `USE_HSPI_PORT`, which moves the TFT onto its own internal SPI object
  bound to HSPI. Reusing one physical SPI peripheral for two devices —
  even via a second `SPIClass` object, even with different pins — isn't
  "sharing a bus," it silently reroutes the peripheral out from under
  whichever device configured it first. An earlier version had SD on
  HSPI too, which worked for short reads (mount, `cardType()`,
  `exists()`) but consistently broke real writes once the TFT was also
  live — the single hardest bug in this project's history to pin down,
  precisely because reads kept working right up until they didn't. **If
  you ever change which SPI port the TFT uses, `sdSPI_` needs to move to
  the other one to match.**
- `SD_SPI_CLOCK_HZ` (55MHz) matches a proven-working reference sketch's
  speed on this exact board. It replaced an initially "safer-sounding"
  4MHz default, though that comparison was confounded at the time by two
  unrelated problems (see below) — treat 55MHz as a reasonable default,
  not a load-bearing fix.
- After a successful `SD.begin()`, checks `cardType()` for `CARD_NONE`
  (distinguishes "no card responded" from "card responded, but with an
  error" — different failure modes, different likely causes, both
  reported to Serial with which one occurred).
- Ensures `/sessions` exists (`SD.exists()`/`SD.mkdir()`), falling back to
  writing session files at the card's **root** if the directory can't be
  created — rather than hard-failing every recording over one
  subdirectory. `logDir_` (`""` = root) records which mode is active for
  `startSession()` to use.
- Finishes with `runWriteSelfTest()` — a real write + read-back of a
  throwaway file (`/cydtest.tmp`), independent of the session/directory
  logic above, specifically to catch a card that mounts and lists files
  fine but can't actually be written to. **One development unit's card
  turned out to be physically read-only** — it passed every read check
  above and still failed every write, unconditionally, in every
  location; a different card resolved it immediately. This self-test
  exists because that failure mode is otherwise indistinguishable from a
  code bug without a second card to compare against.

## 2. Starting a recording: `toggleRecording()` → `SDLogger::startSession()`

`toggleRecording()` (`.ino`), called from `handleTerminalTap()` on a REC
tap (see [TOUCH_INPUT.md](TOUCH_INPUT.md)):
```cpp
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
```
Note there's no separate error UI on failure — `statusText()` (§4) already
reflects *why* on the very same button, so a failed start just... doesn't
change to `REC`, and the existing `NO SD`/`SD OK` label explains itself.

`SDLogger::startSession()`:
1. Calls `mount()` (§1) — always a fresh check, never assumes a prior
   mount is still valid.
2. `settings.nextSessionNumber()` — see §3.
3. Builds the filename from `logDir_` (root or `/sessions`) and the
   session number: `session_0042.log`, zero-padded to 4 digits.
4. Opens it with `FILE_WRITE` (create+truncate) and writes a one-line
   header (`# CYD Serial Terminal session 42 started at millis()=...`)
   before returning — this both confirms the open truly succeeded (a
   `File` handle can sometimes be non-null but non-functional) and gives
   every log file a self-identifying first line.

## 3. Session numbering: `Settings::nextSessionNumber()`

Same `Preferences`/NVS mechanism as the baud rate (see
[BAUD_SELECT.md](BAUD_SELECT.md) §4), under its own key
(`PREFS_KEY_SESSION_NUM`). Monotonically increasing and persisted across
reboots, so file names never collide even after power loss or many
separate sessions — there's no attempt to reuse or fill gaps from
deleted files, by design (simplicity over tidiness).

## 4. While recording: `SDLogger::writeLine()`

Called from `onLineReceived()` (`.ino`) once per completed raw line, only
while `recording` is true — see
[SERIAL_TO_DISPLAY.md](SERIAL_TO_DISPLAY.md) §2. Writes the *same*
timestamp-prefixed string that appears on screen
(`TermBuffer::formatTimestamp(timestampMs) + " " + line`) — not the
word-wrapped, display-formatted version `TermBuffer` stores, so the log
file's line breaks match the original serial traffic, not the screen's
column width.

Flushes are throttled (`SD_LOG_FLUSH_INTERVAL_MS`, 2 seconds) rather than
happening on every line — trades a small worst-case data-loss window (an
unflushed line or two, if power is lost) for much less flash wear and
I/O stalling on a busy line, per `lastFlushMs_`. `stopSession()` always
flushes before closing, so a deliberate REC-to-stop always saves
everything buffered up to that point.

## 5. Status text: `SDLogger::statusText()`

The single function backing the REC button's label, and the whole
reason there's no separate error dialog anywhere in this flow:
```cpp
String SDLogger::statusText() const {
  if (file_) return "REC";
  if (mounted_) return "SD OK";
  return "NO SD";
}
```
Checked (and the button redrawn) via `display.markStatusDirty()` after
every mount attempt and every toggle — never polled continuously, since
nothing changes it outside of those explicit triggers.

## Key files at a glance

| File | Owns |
|---|---|
| `SDLogger.{h,cpp}` | Mounting, session file lifecycle, write self-test |
| `Settings.{h,cpp}` | Session-number persistence (shared class with baud rate) |
| `ESP32_CYD_Serial_Terminal.ino` | `recording` flag, REC tap dispatch, feeding `writeLine()` |

## Tunables

| Constant | Where | Meaning |
|---|---|---|
| `SD_CS/SCLK/MOSI/MISO_PIN` | `Config.h` | microSD wiring (own bus, not shared with the TFT) |
| `SD_SPI_CLOCK_HZ` (55MHz) | `Config.h` | SD SPI clock speed |
| `SD_LOG_DIR` (`/sessions`) | `Config.h` | Preferred session-file directory (falls back to root) |
| `SD_LOG_FLUSH_INTERVAL_MS` (2000) | `Config.h` | How often an active recording flushes to the card |
| `PREFS_KEY_SESSION_NUM` | `Config.h` | NVS key `nextSessionNumber()` uses |
