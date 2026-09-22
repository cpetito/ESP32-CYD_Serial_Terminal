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
| microSD CS (shares TFT's SPI bus)     | 5 |

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
- **XPT2046_Touchscreen** by Paul Stoffregen

Everything else (`Preferences`, `SD`, `SPI`) ships with the ESP32 board
package.

## TFT_eSPI setup (required, one-time)

TFT_eSPI is configured at compile time via its `User_Setup.h` file, not from
the sketch. In your Arduino libraries folder, open
`TFT_eSPI/User_Setup.h` and replace its contents with:

```cpp
#define USER_SETUP_ID 200

#define ILI9341_DRIVER

#define TFT_MISO 12
#define TFT_MOSI 13
#define TFT_SCLK 14
#define TFT_CS    15
#define TFT_DC     2
#define TFT_RST   -1   // not connected on this board
#define TFT_BL    21
#define TFT_BACKLIGHT_ON HIGH

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
```

Do **not** define `TOUCH_CS` here — this sketch drives the XPT2046 itself on
a separate SPI bus (HSPI), independent of TFT_eSPI's own touch support.

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
- **CLR** — clears the on-screen history.
- **AUTO / PAUSED** — auto-scroll indicator/toggle. Dragging the terminal
  area up/down scrolls through history and automatically pauses
  auto-scroll; tap this button (or drag back to the bottom) to resume.

## Touch calibration

The XPT2046 digitizer on this board sits in its native portrait
orientation regardless of the display's landscape rotation, so its raw X
and Y readings come in swapped relative to the screen. `Config.h` accounts
for this by default (`TOUCH_SWAP_XY 1`). Symptom of this being wrong: only
touches in one narrow band (e.g. just the rightmost status-bar button)
register, while the rest of that same row does nothing — that's a swapped-
axis bug, not a fine calibration offset.

If, after this default, touches still feel off, work through it in order:

1. **Wrong axis entirely** (e.g. dragging left-right on screen moves the
   scroll position, which should only respond to up-down drags) — flip
   `TOUCH_SWAP_XY` to `0`.
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

Tap the four screen corners and check the mapped coordinates land near
`(0,0)`, `(319,0)`, `(0,239)`, and `(319,239)` respectively; use any
mismatch to tell which of the three adjustments above you still need.

## Project layout

```
ESP32_CYD_Serial_Terminal/
  ESP32_CYD_Serial_Terminal.ino   setup()/loop(), touch dispatch, wiring
  Config.h                        pins, colors, tunables
  SerialCapture.{h,cpp}           non-blocking UART line reader
  TermBuffer.{h,cpp}              timestamping, word-wrap, scroll history
  DisplayUI.{h,cpp}               status bar + terminal rendering
  TouchInput.{h,cpp}              XPT2046 read + screen-coordinate mapping
  BaudMenu.{h,cpp}                touch baud-select overlay
  Settings.{h,cpp}                Preferences (baud rate, session counter)
  SDLogger.{h,cpp}                microSD session recording
  UITypes.h                       shared Rect type for hit-testing
```
