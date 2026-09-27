# SD status text logic

A technical reference for how the REC button decides what to say.
Companion to [SD_LOGGING.md](SD_LOGGING.md) (mounting and the session
lifecycle that sets the state this reads) and the other flow docs;
written for whoever next needs to touch this code without re-deriving
it from scratch.

## The logic itself

```cpp
String SDLogger::statusText() const {
  if (file_) return "REC";
  if (mounted_) return "SD OK";
  return "NO SD";
}
```
Three states, from a two-flag precedence check — deliberately in this
order:
- **`REC`** — `file_` is a currently-open `File` handle (truthy via its
  `operator bool()`), meaning a session is actively being written to.
  Checked first because an open file handle implies the card was
  mounted at some point to get here; there's no need to also check
  `mounted_` once `file_` is truthy.
- **`SD OK`** — no open file, but the last `mount()` call succeeded
  (`mounted_ == true`). The card is present and usable, just not
  currently recording.
- **`NO SD`** — neither: either `mount()` has never succeeded, or its
  most recent attempt failed.

This function is a pure read of two `bool`/`File` fields — it does not
itself touch the SD card, retry anything, or have any side effects. It
answers "what do these two flags currently say," nothing more.

## Important: this function was never the bug

It's worth being precise about this, because it's easy to assume a
"status text was wrong" report points here. **`statusText()`'s logic has
been correct since it was first written; the bug that made it report
stale information lived entirely in what set `mounted_`, not in this
function.**

Originally, `SDLogger::mount()` cached success (`if (mounted_) return
true;` as its first line) — so once a card had mounted, `mounted_` never
went back to `false` even if the card was later physically removed, and
`statusText()` kept faithfully reporting `SD OK`, exactly as its own
logic says it should for `mounted_ == true`. The fix
(see [SD_LOGGING.md](SD_LOGGING.md) §1) was making `mount()` always
re-verify the card is genuinely present on every call, rather than
trusting a cached flag — `statusText()` itself was never touched. If a
future report says the status text is wrong, look first at *when*
`mounted_`/`file_` last changed and why, not at these three lines.

## Where it's read

`statusText()` is called from exactly one place, inside `loop()`:
```cpp
display.render(history, capture.baudRate(), sdLogger.statusText(), recording, history.isAutoscrollOn());
```
— re-evaluated on every throttled render pass (see
[SERIAL_TO_DISPLAY.md](SERIAL_TO_DISPLAY.md) §4 for the render throttle),
not cached or polled independently. It's cheap enough (two field reads)
that there's no need to gate it behind the dirty-flag system the way
actual TFT drawing is — only the *drawing* of the REC button is
dirty-flag-gated (`markStatusDirty()`), not the read of what text to
draw when that redraw happens.

`DisplayUI::drawStatusBar()` uses the returned string as the REC
button's label, and separately uses the `.ino`'s own `recording` bool
(passed as a **separate** parameter, not derived from the string) to
decide the button's background color:
```cpp
drawButtonLabel(tft_, btnRec_, recStatus, recording ? COLOR_BTN_BG_ACTIVE : COLOR_BTN_BG, COLOR_BTN_TEXT);
```

## A duplication worth knowing about

`recStatus` (from `sdLogger.statusText()`, backed by `SDLogger`'s own
`file_`/`mounted_`) and `recording` (the `.ino`'s own separate `static
bool`, set in `toggleRecording()`) are **two independent pieces of
state**, not one value read twice. They're kept consistent only by
convention: `toggleRecording()` sets `recording = true` if and only if
`startSession()` returned true, which only happens if `file_` actually
got opened — so in practice `recStatus == "REC"` and `recording == true`
always agree, and the reverse (stopping) is equally paired via
`stopSession()` + `recording = false`.

This is the same *shape* of risk that the AUTO/PAUSED bug (see
[TOUCH_INPUT.md](TOUCH_INPUT.md)) turned out to be — a UI-facing bool
mirroring model state instead of the model being the single source of
truth — but it hasn't caused a problem here, because `toggleRecording()`
is the *only* place either side is ever set, and always sets both
together. If that ever changes (e.g. some new code path starts or stops
a recording without going through `toggleRecording()`), this duplication
stops being safe and `recording` should be replaced with something read
directly from `SDLogger` (e.g. its existing `isRecording()`), the same
way the autoscroll fix consolidated onto `TermBuffer` as the one owner.

## Key files at a glance

| File | Owns |
|---|---|
| `SDLogger.cpp` | `statusText()` itself, and the `mounted_`/`file_` state it reads |
| `ESP32_CYD_Serial_Terminal.ino` | The separate `recording` bool, kept in sync by `toggleRecording()` |
| `DisplayUI.cpp` | Consumes both to draw the REC button's label and color |
