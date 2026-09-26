# ESP32 CYD Serial Terminal / Logger

A receive-only serial monitor and logger for the **ESP32-2432S028R** "Cheap
Yellow Display" (CYD) board — 2.8" ILI9341 TFT + XPT2046 resistive touch —
built as an Arduino IDE sketch.

## Features

- Receive-only serial monitor on **GPIO35**
- Non-blocking serial reception (never blocks `loop()`)
- CR, LF, or CR/LF all terminate a line (CR/LF collapsed into one)
- `millis()` timestamp on every line (`[mm:ss.mmm]`)
- Long lines are word-wrapped on the display
- Scrolling history (drag up/down on screen; auto-scroll pauses while
  scrolled back, and can be resumed with the AUTO/PAUSED button)
- Touch-selectable baud rate, from a full-screen menu
- Baud rate persisted in ESP32 Preferences (NVS), restored on boot
- Optional microSD session recording, toggled with the REC button
- Each recording starts a brand-new, sequentially numbered session file
  (`/sessions/session_0001.log`, `0002`, ...)

## Hardware

Board: **ESP32-2432S028R** (2.8" ILI9341 320x240 + XPT2046 touch + microSD).

| Signal              | GPIO |
|---------------------|------|
| Monitored serial RX | 35   |
| TFT MISO / MOSI / SCLK / CS / DC / BL | 12 / 13 / 14 / 15 / 2 / 21 |
| Touch CLK / MOSI / MISO / CS / IRQ    | 25 / 32 / 39 / 33 / 36 |
| microSD CS / SCLK / MOSI / MISO       | 5 / 18 / 23 / 19 |

**GPIO35 is input-only**, which is exactly what an RX-only monitor needs,
and it isn't used by the display, touch, or SD hardware on this board — it's
broken out on the CN1 header.

**Wiring the monitored device:** connect its TX pin to the CYD's GPIO35, and
tie the grounds together. The ESP32 is **3.3V logic only** — if you're
monitoring a 5V device (e.g. an Arduino Uno), add a level shifter or a
resistor divider (e.g. 10kΩ/20kΩ) between its TX pin and GPIO35, or you can
damage the input.

## Required libraries (Arduino Library Manager)

- **TFT_eSPI** by Bodmer

Everything else (`Preferences`, `SD`, `SPI`) ships with the ESP32 board
package. Touch is driven by `SoftSPI` + `XPT2046_TouchscreenSOFTSPI` —
bit-banged (software) SPI, vendored directly into this sketch folder
(sourced from RandomNerdTutorials' CYD example, since that fork isn't in
the Library Manager) rather than a hardware-SPI touch library; see
[Why touch is bit-banged](#why-touch-is-bit-banged) below.

## TFT_eSPI setup (required, one-time)

TFT_eSPI is configured at compile time via its `User_Setup.h` file, not from
the sketch. In your Arduino libraries folder, open
`TFT_eSPI/User_Setup.h` and replace its contents with:

```cpp
#define USER_SETUP_ID 200

#define ILI9341_2_DRIVER   // not ILI9341_DRIVER - see Bodmer/TFT_eSPI#1172

#define TFT_MISO 12
#define TFT_MOSI 13
#define TFT_SCLK 14
#define TFT_CS    15
#define TFT_DC     2
#define TFT_RST   -1   // not connected on this board
#define TFT_BL    21
#define TFT_BACKLIGHT_ON HIGH

#define TOUCH_CS 33   // harmless if defined - this sketch drives touch itself,
                       // never through TFT_eSPI's own touch support

#define SPI_FREQUENCY        55000000
#define SPI_READ_FREQUENCY   20000000
#define SPI_TOUCH_FREQUENCY   2500000

#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_FONT6
#define LOAD_FONT7
#define LOAD_FONT8
#define LOAD_GFXFF
#define SMOOTH_FONT

// Critical on this board: moves the TFT onto HSPI, leaving VSPI free for
// the microSD card. Without this, the TFT and SD card silently fight over
// the same physical SPI peripheral - reads mostly get away with it, but
// writes fail consistently. See "Why touch is bit-banged" below.
#define USE_HSPI_PORT
```

This is confirmed working (not just a generic guess) — it's the actual
`User_Setup.h` in use on the hardware this sketch was debugged against,
which itself follows the RandomNerdTutorials CYD guide's own required file
rather than a generic community one (their guide is explicit that a
generic `User_Setup.h` "will probably NOT work").

## Arduino IDE board settings

- Board: **ESP32 Dev Module** (or your CYD-specific variant if your board
  package includes one)
- Flash size: 4MB (or more, matching your board)
- Partition scheme: Default
- Upload speed: 921600 is usually fine

## Using it

The top status bar has four touch buttons:

- **Baud rate** (e.g. `115200`) — opens a full-screen menu to pick a new
  baud rate; it's saved immediately and used on next boot too.
- **REC** — toggles microSD session recording. Shows `NO SD` if no card is
  present, `SD OK` once mounted, and `REC` (highlighted) while recording.
  Card presence is re-checked for real on every tap (not just once at
  boot), so removing the card and tapping REC correctly falls back to
  `NO SD` rather than continuing to show a stale `SD OK`.
- **CLR** — clears the on-screen history.
- **AUTO / PAUSED** — auto-scroll indicator/toggle. Dragging the terminal
  area up/down scrolls through history and automatically pauses
  auto-scroll; tap this button (or drag back to the bottom) to resume.

## Why touch is bit-banged

This board needs three independent SPI buses: the TFT (SCLK/MOSI/MISO on
12/13/14), touch (25/32/39), and the microSD card (18/23/19) — three
genuinely different sets of pins, confirmed by testing. The ESP32 classic
only has **two** general-purpose hardware SPI peripherals available for
user code (commonly called HSPI and VSPI).

Two conflicts had to be found and fixed, in turn, before all three
devices had a bus genuinely their own:

1. **SD vs. whichever peripheral it was pointed at.** An earlier version
   of this sketch gave the TFT and touch each a hardware peripheral and
   then pointed the SD card at the *same* peripheral some other device
   already owned, just re-initialized with different pins — which doesn't
   share a bus, it reroutes the physical peripheral out from under
   whichever device configured it first. Short reads (mounting,
   `cardType()`, `exists()`) were fast enough to get away with it; real
   write (and eventually even read) sequences weren't, and failed
   consistently once the other device was also active.
2. **Which peripheral is actually free.** This board's required
   `User_Setup.h` (see above) defines `USE_HSPI_PORT`, moving the TFT onto
   **HSPI** rather than the default VSPI. `SDLogger` originally assumed
   the opposite and put the SD card on HSPI too, recreating conflict #1
   in a new spot. `SDLogger` now uses **VSPI** for the SD card
   specifically because it's the peripheral `USE_HSPI_PORT` leaves
   unclaimed on this setup — if your `User_Setup.h` *doesn't* define
   `USE_HSPI_PORT`, the TFT is on VSPI instead, and `sdSPI_` in
   `SDLogger.h` needs to move to `SPIClass(HSPI)` to match.

That still leaves three devices needing buses but only two hardware
peripherals to go around. The resolution: touch — the lowest-bandwidth of
the three, comfortably fine with software timing at normal poll rates —
is driven by `SoftSPI` + `XPT2046_TouchscreenSOFTSPI` (vendored unmodified
into this sketch folder from RandomNerdTutorials' own CYD
display+touch+microSD example, rather than hand-rolled — the exact
XPT2046 bit timing is a hardware detail that's easy to get subtly wrong
without a scope to verify against, which is exactly what happened on the
first attempt here) instead of a hardware SPI peripheral at all. That
frees a whole hardware peripheral for the SD card's **exclusive** use,
while the TFT keeps its own peripheral (HSPI, per `USE_HSPI_PORT`)
untouched. All three devices now have a bus that's genuinely theirs
alone.

`TouchInput` sets this library's rotation to `1` (its passthrough case),
so the raw ADC values it returns are the true, unprocessed reading — same
as a plain hardware-SPI XPT2046 library would give — meaning the existing
`TOUCH_SWAP_XY`/`TOUCH_INVERT_*`/`TOUCH_RAW_*_MIN/MAX` calibration below
still means exactly what it meant before; if values look off, redo the
corner-tap calibration to confirm.

## Touch calibration

By default the raw XPT2046 X/Y readings map directly to the screen (no
swap, no mirroring) — confirmed against this board via `TOUCH_DEBUG_SERIAL`
readings, so `TOUCH_SWAP_XY`/`TOUCH_INVERT_X`/`TOUCH_INVERT_Y` in `Config.h`
all default to off. The main source of missed taps in practice is a small
Y-axis offset in the default `TOUCH_RAW_Y_MIN/MAX` calibration, which
`STATUS_BAR_HEIGHT` (32px, taller than a single text row) is sized to
tolerate.

If touches still feel off on your specific panel, work through it in
order:

1. **Wrong axis entirely** (e.g. dragging left-right on screen moves the
   scroll position, which should only respond to up-down drags) — flip
   `TOUCH_SWAP_XY` to `1`.
2. **Right axis, but mirrored** (e.g. tapping the left button hits the
   right one instead, or the top of the screen responds to bottom taps) —
   set `TOUCH_INVERT_X` and/or `TOUCH_INVERT_Y` to `1` as needed.
3. **Close but offset** — adjust `TOUCH_RAW_X_MIN/MAX` and
   `TOUCH_RAW_Y_MIN/MAX` to match your panel's actual raw range.

To see live numbers while adjusting any of the above, set
`TOUCH_DEBUG_SERIAL` to `1` in `Config.h`, re-upload, and open the Serial
Monitor at 115200 baud. Each touch prints its raw XPT2046 reading and the
resulting mapped screen coordinate, e.g.:

```
touch raw=(612,3421) mapped=(215,17)
```

A tap that jitters past `DRAG_THRESHOLD_PX` during the first
`DRAG_MIN_HOLD_MS` of contact (both in the `.ino`) is still treated as a
tap, not a drag, to absorb the noisy first reading or two typical of a
resistive panel settling. If taps are still occasionally swallowed on your
panel, raising `DRAG_THRESHOLD_PX` (pixels) or `DRAG_MIN_HOLD_MS`
(milliseconds) further is safe — a real drag gesture moves much further
over a much longer hold than either guards against.

Tap the four screen corners and check the mapped coordinates land near
`(0,0)`, `(319,0)`, `(0,239)`, and `(319,239)` respectively; use any
mismatch to tell which of the three adjustments above you still need.

## SD card issues

`SDLogger::mount()` (called when you tap REC) logs exactly where it failed
to the USB Serial Monitor (115200 baud) — open it and tap REC to see one
of:

- **`SD.begin() failed`** — the SPI transaction to the card itself never
  succeeded. Check `SD_CS_PIN`/`SD_SCLK_PIN`/`SD_MOSI_PIN`/`SD_MISO_PIN` in
  `Config.h` against your board. **These vary across CYD units**: some
  share the TFT's SPI bus (12/13/14), others wire the microSD slot to the
  ESP32's separate default VSPI pins (18/23/19, `SD_CS_PIN` still `5`) —
  the latter is what this sketch now defaults to, confirmed by testing.
  If your card still won't mount, verify with a multimeter/continuity
  check against the card slot's pads. Also confirm the card is fully
  seated (listen for the click).
- **`no card was detected (cardType() == CARD_NONE)`** — the SPI
  transaction succeeded but the card itself didn't respond, almost always
  a seating issue: reseat the card or try a different one.
- **`mounted OK (cardType=..., N MB)`** followed by **`mkdir(...) failed`**
  — the card mounted (correct size/type reported) but can't create a
  directory. If the card is a fresh 16GB+ one you haven't verified
  elsewhere, this can mean it's preformatted **exFAT** rather than
  FAT16/FAT32 (this Arduino SD library only supports the latter) —
  reformat it as FAT32 if so (Windows: right-click the drive → Format →
  FAT32; or the SD Association's free "SD Card Formatter" tool, more
  reliable than most OS format dialogs for larger cards). But if you've
  already confirmed the card is FAT32 and read/writable from a PC, keep
  reading — the write self-test below is the more useful signal.
- **`write self-test FAILED`** (right after the mount line, always run
  once per mount) — a direct, path-independent write + read-back at the
  card's root. If this fails (or the mkdir/session-file opens above fail)
  **on a card you've already confirmed is FAT32 and writable from a PC**,
  the filesystem isn't the problem — reads (mount, `cardType()`,
  `cardSize()`, `exists()`) all go through the SPI bus fine, but every
  write fails. On this exact board, that turned out to be an SD-vs-TFT
  hardware SPI peripheral conflict (see
  [Why touch is bit-banged](#why-touch-is-bit-banged)) - specifically
  `SDLogger` assuming the wrong one of HSPI/VSPI was free. A telling sign
  if you hit this on a fresh setup: it isn't only writes that fail, but
  *any* SD access performed after the TFT/touch have been initialized,
  while access performed before they're initialized (e.g. very early in
  `setup()`) works fine — a card that mounts and passes reads/writes in
  isolation but fails once other SPI peripherals are also live points
  straight at this, not at power or the card itself. If it's *still*
  failing after confirming `sdSPI_` (`SDLogger.h`) uses whichever
  peripheral your `User_Setup.h` *doesn't* claim, next suspects are:
  - **The card itself**: one card used during this sketch's development
    turned out to be physically read-only — it mounted, reported a
    correct size/type, and passed reads, but failed every write, in every
    location, unconditionally. A different card resolved it immediately.
    Worth ruling out early with a second card if nothing else here fits.
  - **Power**: SD writes draw a real current spike beyond what reads need,
    and the TFT backlight + SD drawing from it simultaneously can exceed
    what a marginal USB cable/port delivers on these boards. Try a
    different (short, quality) USB cable/port, or power the board from a
    proper 5V/1A+ supply rather than a laptop USB port.
  - **Seating/wiring**: a connection solid enough for the lighter-weight
    read commands but marginal under a write command's timing — reseat
    the card, try a different one, and if you're not on stock wiring,
    check for loose/long jumper wires on the SD lines.
- **`write self-test passed`** — the card can be written to; if session
  recording still fails after this, the problem is specific to the
  session file's own path, not the card/power/wiring in general.

`mount()` uses `SD_SPI_CLOCK_HZ` (`Config.h`, currently 55MHz) — matched to
the RandomNerdTutorials reference sketch's proven-working speed on this
board. This replaced an initially "safer-sounding" 4MHz default, though
that change was never cleanly confirmed necessary: the write failures
seen at the time turned out to have two other explanations (a physically
read-only card, and the SD/TFT SPI peripheral conflict below), either of
which would fail at any clock speed. Matching the reference's proven
value is a reasonable default regardless; lower it if writes become
unreliable on a particular card, since a very low clock isn't a
universally safe choice either (it holds each bit on the line for a much
longer window, which can give noise more time to corrupt it).

## Project layout

```
ESP32_CYD_Serial_Terminal/
  ESP32_CYD_Serial_Terminal.ino   setup()/loop(), touch dispatch, wiring
  Config.h                        pins, colors, tunables
  SerialCapture.{h,cpp}           non-blocking UART line reader
  TermBuffer.{h,cpp}              timestamping, word-wrap, scroll history
  DisplayUI.{h,cpp}               status bar + terminal rendering
  TouchInput.{h,cpp}              XPT2046 read (via SoftSPI) + coordinate mapping
  BaudMenu.{h,cpp}                touch baud-select overlay
  Settings.{h,cpp}                Preferences (baud rate, session counter)
  SDLogger.{h,cpp}                microSD session recording
  UITypes.h                       shared Rect type for hit-testing
  SoftSPI.{h,cpp}                 vendored bit-banged SPI (RandomNerdTutorials)
  XPT2046_TouchscreenSOFTSPI.{h,cpp}  vendored SoftSPI-based touch driver
```

For how a byte on GPIO35 becomes pixels on screen - the
`SerialCapture` → `TermBuffer` → `DisplayUI` pipeline, timestamping,
word-wrap, and the ring buffer/autoscroll interaction - see
[SERIAL_TO_DISPLAY.md](SERIAL_TO_DISPLAY.md).
