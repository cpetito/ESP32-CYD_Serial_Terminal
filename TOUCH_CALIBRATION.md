# Touch calibration flow

A technical reference for how a raw XPT2046 ADC reading becomes a
screen coordinate, the diagnostic tool for checking that mapping, and
the procedure for fixing it on a different panel. Companion to
[TOUCH_INPUT.md](TOUCH_INPUT.md) (which covers the gesture state
machine downstream of this) and the others; written for whoever next
needs to touch this code without re-deriving it from scratch. Read the
[case study](#case-study-a-false-diagnosis) below before assuming a
future "touches feel miscalibrated" report is actually a calibration
problem — on this exact board, it wasn't.

## The model

`TouchInput::getPoint()` (`TouchInput.cpp`) turns one raw XPT2046
reading (`TS_Point p`, each axis a 12-bit ADC value, `0`–`4095`) into a
screen pixel coordinate in three steps:

```cpp
// 1. Which raw axis feeds screen X vs Y
#if TOUCH_SWAP_XY
  long rawX = p.y, rawXMin = TOUCH_RAW_Y_MIN, rawXMax = TOUCH_RAW_Y_MAX;
  long rawY = p.x, rawYMin = TOUCH_RAW_X_MIN, rawYMax = TOUCH_RAW_X_MAX;
#else
  long rawX = p.x, rawXMin = TOUCH_RAW_X_MIN, rawXMax = TOUCH_RAW_X_MAX;
  long rawY = p.y, rawYMin = TOUCH_RAW_Y_MIN, rawYMax = TOUCH_RAW_Y_MAX;
#endif

// 2. Linear map from that axis's calibrated raw range to screen pixels,
//    optionally reversed
#if TOUCH_INVERT_X
  long mx = map(rawX, rawXMin, rawXMax, screenW_, 0);
#else
  long mx = map(rawX, rawXMin, rawXMax, 0, screenW_);
#endif
// ...same for my/TOUCH_INVERT_Y...

// 3. Clamp to the panel
mx = constrain(mx, 0, screenW_ - 1);
my = constrain(my, 0, screenH_ - 1);
```

Four `Config.h` constants control this, and they answer four
**independent** questions — conflating them is the most common way to
chase the wrong fix:

| Constant | Question it answers | Wrong value looks like |
|---|---|---|
| `TOUCH_SWAP_XY` | Does raw X drive screen X, or screen Y? | Dragging left-right scrolls the terminal (which should only respond to up-down drags), or vice versa |
| `TOUCH_INVERT_X` | Does raw X increase left-to-right, or right-to-left? | Tapping the left button hits the right one (X only) |
| `TOUCH_INVERT_Y` | Does raw Y increase top-to-bottom, or bottom-to-top? | The top of the screen responds to bottom taps (Y only) |
| `TOUCH_RAW_X/Y_MIN/MAX` | What raw values correspond to the panel's physical edges? | Right axis, right direction, but taps land a consistent few pixels off |

On this board, confirmed by testing (see the case study), none of the
first three need to change from their defaults (`0`/`0`/`0` — no swap,
no inversion). Don't assume that's universal for every CYD unit; it's
just what this specific panel needed.

## The diagnostic: `TOUCH_DEBUG_SERIAL`

Set to `1` in `Config.h`, re-upload, open the Serial Monitor at 115200
baud. `TouchInput::getPoint()` gains one extra line, printed on every
successful touch read, after all the mapping above has already run:
```cpp
#if TOUCH_DEBUG_SERIAL
  Serial.printf("touch raw=(%d,%d) mapped=(%d,%d)\n", p.x, p.y, x, y);
#endif
```
`raw=` is `p.x`/`p.y` exactly as `XPT2046_TouchscreenSOFTSPI::getPoint()`
returned them — **before** any swap/invert/calibration is applied — so
this line shows you both ends of the pipeline in one place: what the
hardware actually reported, and what this sketch decided that meant.
Comparing the two, across several taps at known screen locations, is
how you tell the four failure modes above apart (see the procedure
below) instead of guessing.

Leave this off (`0`) for normal use — it's a per-touch `Serial.printf()`,
harmless but noisy, and one more reason a "why does the terminal feel
laggy" report might really be about a debug flag left on rather than
the serial pipeline itself.

## Calibration procedure

1. Enable `TOUCH_DEBUG_SERIAL` (above).
2. Tap each of the four screen corners in turn and read the `mapped=`
   values back from Serial. They should land near `(0,0)`, `(319,0)`,
   `(0,239)`, and `(319,239)` respectively (this board is 320×240,
   landscape).
3. Diagnose from the pattern, worst problem first:
   - **Wrong axis entirely** — e.g. the value that should track screen X
     stays roughly constant across corners while the value that should
     track screen Y swings wildly, or a drag gesture moves the wrong way
     — set `TOUCH_SWAP_XY` to `1`.
   - **Right axis, but the direction is backwards** — e.g. `mapped=`'s X
     decreases as you tap further right — set `TOUCH_INVERT_X` and/or
     `TOUCH_INVERT_Y` to `1`, whichever axis is reversed.
   - **Right axis, right direction, consistently offset** — e.g. every
     corner reads a similar number of pixels short or long — this is a
     `TOUCH_RAW_*_MIN/MAX` problem, not a swap/invert one. Use the `raw=`
     values from the corner taps directly: the value at the physical
     left edge is your new `TOUCH_RAW_X_MIN`, the value at the right
     edge is `TOUCH_RAW_X_MAX` (same idea for Y) — you're re-deriving the
     two endpoints `map()` interpolates between, not guessing at them.
4. Re-upload after each change and re-check — these interact (an axis
   swap changes what "the X reading" even means), so confirm one, then
   move to the next.
5. Disable `TOUCH_DEBUG_SERIAL` again once satisfied.

## Case study: a false diagnosis

Worth reading before repeating it. Early in this project, exactly one of
four adjacent status-bar buttons responded to touch — the other three,
sitting in the same row, did nothing. That pattern (one button in a row
works, the rest don't) was first diagnosed as a **swapped axis**: a
plausible-sounding theory, since a swap can make a horizontal row of
targets behave inconsistently depending on where the (wrongly-assigned)
other axis happens to land. Applying `TOUCH_SWAP_XY = 1` on that theory
made things *worse* — now every button failed, including the one that
had worked.

The actual fix came from working backwards from two real `raw=`/`mapped=`
data points (one per button tap) using the model above in reverse: given
the *known* physical location of each button and the *observed*
`mapped=` output, solve for what `map()` must have been fed to produce
that output, and check whether raw X tracked physical X (it did) and raw
Y tracked physical Y (it also did — no swap needed at all). Once the
axes were confirmed correct, reversing the incorrect swap change and
re-examining the *original* symptom pointed at the true cause: the
button row was only 18 pixels tall, and normal calibration slop (a few
pixels either way — never fully eliminated on a resistive panel) was
occasionally enough to land just outside such a thin target. The fix
was `STATUS_BAR_HEIGHT` (`Config.h`), not any of the four constants
above — it's `32`px now specifically to give real tolerance for
calibration imprecision, rather than chasing pixel-perfect calibration
to compensate for a target that was simply too thin.

The lesson generalizes: **a small number of buttons failing while others
work is at least as likely to be a hit-target-size problem as a
calibration problem**, and the two are easy to conflate because both
present as "some taps don't register." If you hit this again, check
target sizes (`DisplayUI.cpp`'s button `Rect`s, `STATUS_BAR_HEIGHT`)
*before* re-touching the calibration constants — and if you do suspect
calibration, use the reverse-engineering technique above (real
coordinates in, known physical location out) rather than guessing at a
swap/invert combination and re-flashing to see if it helps.

A related, but distinct, failure mode: taps that register at the
*correct* coordinates but still get dropped are a debounce problem
(`DRAG_THRESHOLD_PX`/`DRAG_MIN_HOLD_MS`), not a calibration one — see
[TOUCH_INPUT.md](TOUCH_INPUT.md)'s gesture state machine section.

## Key files at a glance

| File | Owns |
|---|---|
| `TouchInput.cpp` | The mapping itself (`getPoint()`) and the debug print |
| `Config.h` | All four calibration constants, `TOUCH_DEBUG_SERIAL`, `STATUS_BAR_HEIGHT` |
| `DisplayUI.cpp` | Button `Rect` sizes/positions — the other half of "why didn't that tap register" |

## Tunables

| Constant | Meaning |
|---|---|
| `TOUCH_SWAP_XY` | Which raw axis feeds screen X vs Y (`0` on this board) |
| `TOUCH_INVERT_X` / `TOUCH_INVERT_Y` | Direction of each axis (`0`/`0` on this board) |
| `TOUCH_RAW_X_MIN/MAX`, `TOUCH_RAW_Y_MIN/MAX` | Raw ADC values at each axis's physical edges |
| `TOUCH_DEBUG_SERIAL` | Enables the per-touch raw+mapped Serial print |
| `STATUS_BAR_HEIGHT` (32) | Button hit-target height — tolerance for calibration slop, not itself a calibration constant |
