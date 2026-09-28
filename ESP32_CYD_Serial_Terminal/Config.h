// Config.h
// Board: ESP32-2432S028R "Cheap Yellow Display" (CYD)
// 2.8" ILI9341 320x240 TFT + XPT2046 resistive touch
//
// Central place for pin assignments, colors and tunable constants.

#pragma once

#include <Arduino.h>

// ---------------------------------------------------------------------------
// Monitored serial line: RX (always) + TX (optional, for the canned-message
// SEND menu)
// ---------------------------------------------------------------------------
// GPIO22 is broken out to this board's P3 connector (confirmed on the
// actual hardware, alongside 21/27/35 - 21 is used elsewhere, as the TFT
// backlight pin) and isn't used by the TFT, touch controller or microSD
// slot, making it a convenient RX pin for sniffing a target device's TX
// line. Unlike GPIO35 (used here originally), GPIO22 is a regular
// bidirectional GPIO with real internal pull-up/pull-down support, so
// SerialCapture::begin() enables its internal pull-up: with nothing
// connected, the line idles high (the UART's normal mark state) instead
// of floating and occasionally having noise framed as spurious bytes -
// no external resistor needed, unlike on GPIO35/34/36/39 (the ESP32's
// four input-only pins, none of which have any internal pull at all).
// NOTE: ESP32 GPIOs are 3.3V only. If the device being monitored uses 5V
// logic (e.g. an Arduino Uno), use a level shifter or a resistor divider
// (e.g. 10k/20k) between its TX pin and this pin to avoid damaging the
// ESP32 - the internal pull-up above doesn't change that requirement.
#define MONITOR_RX_PIN      22
// GPIO27 is the other pin confirmed free on the P3 connector. GPIO35 -
// otherwise the obvious second choice, since it's also on P3 and unused -
// can NOT be used here: it's one of the ESP32's input-only pins (34/35/36/39),
// which have no output driver at the silicon level, not just a missing
// pull resistor. Trying to use it as TX wouldn't error, it would just
// never actually transmit anything.
#define MONITOR_TX_PIN      27
#define MONITOR_UART        Serial2
#define MONITOR_UART_NUM    2

// Canned messages offered on the touch SEND menu, each transmitted out
// MONITOR_TX_PIN followed by TX_LINE_ENDING. Add/remove/edit entries and
// re-flash to change what's offered - there's no on-screen text entry.
#define TX_LINE_ENDING "\r\n"
struct TxMessage {
  const char *label; // shown on the SEND menu's button
  const char *text;  // sent verbatim, followed by TX_LINE_ENDING
};
static const TxMessage TX_MESSAGES[] = {
  { "RESET",  "RESET"  },
  { "STATUS", "STATUS" },
};
#define TX_MESSAGES_COUNT (sizeof(TX_MESSAGES) / sizeof(TX_MESSAGES[0]))

// ---------------------------------------------------------------------------
// TFT (ILI9341) - driven by the TFT_eSPI library.
// Pin wiring itself is configured at compile time inside the TFT_eSPI
// library's User_Setup.h (see README.md for the exact block to paste in).
// The backlight pin is controlled directly from this sketch.
// ---------------------------------------------------------------------------
#define TFT_BL_PIN           21
#define TFT_BACKLIGHT_ON     HIGH

// ---------------------------------------------------------------------------
// Touch (XPT2046) - deliberately driven by SoftSPI (bit-banged, software
// SPI - see SoftSPI.h/XPT2046_TouchscreenSOFTSPI.h) rather than a hardware
// SPI peripheral. This board needs three independent SPI buses (TFT,
// touch, microSD - see the microSD section below for why they can't
// share), but the ESP32 classic only has two hardware SPI peripherals;
// bit-banging touch (the lowest-bandwidth of the three) frees a whole
// peripheral for the SD card.
// ---------------------------------------------------------------------------
#define TOUCH_CLK_PIN        25
#define TOUCH_MOSI_PIN       32
#define TOUCH_MISO_PIN       39
#define TOUCH_CS_PIN         33
#define TOUCH_IRQ_PIN        36

// Raw ADC calibration range measured from the touch controller. These are
// the commonly used defaults for this board; tweak if touches feel offset.
#define TOUCH_RAW_X_MIN      200
#define TOUCH_RAW_X_MAX      3700
#define TOUCH_RAW_Y_MIN      240
#define TOUCH_RAW_Y_MAX      3800

// Whether the touch controller's raw X/Y axes need to be swapped or
// mirrored to line up with the landscape (rotation 1) screen. Confirmed
// via TOUCH_DEBUG_SERIAL readings that this board's raw p.x tracks screen
// X and raw p.y tracks screen Y directly - no swap. Left here as a knob
// in case a different panel/board revision needs it: if touches land on
// the wrong axis, set TOUCH_SWAP_XY to 1; if the right axis but mirrored,
// flip the matching TOUCH_INVERT_* instead of editing the mapping code.
#define TOUCH_SWAP_XY         0
#define TOUCH_INVERT_X        0
#define TOUCH_INVERT_Y        0

// Set to 1 and open the Serial Monitor (115200) to print each touch's raw
// and mapped coordinates - handy for re-deriving TOUCH_RAW_*_MIN/MAX or
// checking the swap/invert flags above against your specific panel.
#define TOUCH_DEBUG_SERIAL    0

// ---------------------------------------------------------------------------
// microSD card slot. Confirmed via testing that on this unit it's wired to
// its own separate bus (the ESP32's default VSPI pins) rather than sharing
// the TFT's SCLK/MOSI/MISO (12/13/14) - if your card won't mount, that's
// the first thing to try swapping. SDLogger drives it on its own dedicated
// hardware SPI peripheral, kept deliberately separate from whichever one
// TFT_eSPI's User_Setup.h assigns to the display (USE_HSPI_PORT on this
// board's required setup - see SDLogger.h) - reusing one physical
// peripheral for both, even via a second SPIClass object, doesn't "share"
// a bus, it reroutes the peripheral out from under whichever device
// configured it first, which silently corrupted SD access once both were
// live (isolated single-peripheral reads got lucky; sustained access,
// like real writes, consistently didn't).
// ---------------------------------------------------------------------------
#define SD_CS_PIN            5
#define SD_SCLK_PIN          18
#define SD_MOSI_PIN          23
#define SD_MISO_PIN          19

// Matches the RandomNerdTutorials reference sketch's proven-working speed
// on this board. This was changed up from an initially "safer-sounding"
// 4MHz on the theory that a very low SPI clock holds each bit's voltage
// on the line for a much longer window, giving noise more time to corrupt
// it than a faster clock does - plausible, but never cleanly confirmed:
// the actual write failures at the time turned out to be caused by an SD
// card that was physically read-only, and separately by an SD/TFT SPI
// peripheral conflict (see SDLogger.h), either of which would have failed
// at any clock speed. Matching the reference's value is a reasonable
// default regardless; lower it if writes become unreliable on a
// particular card.
#define SD_SPI_CLOCK_HZ      55000000

// ---------------------------------------------------------------------------
// Display geometry / terminal layout
// ---------------------------------------------------------------------------
#define SCREEN_ROTATION      1     // landscape, USB/header on the right
#define TERM_TEXT_SIZE       1     // GLCD font (font 1), 6x8 px per glyph at size 1
#define CHAR_W                6
#define CHAR_H                8
#define STATUS_BAR_HEIGHT    32   // taller than one glyph row - forgiving touch target

// ---------------------------------------------------------------------------
// Colors
// ---------------------------------------------------------------------------
#define COLOR_BG             TFT_BLACK
#define COLOR_TEXT           TFT_GREEN
#define COLOR_TIMESTAMP      TFT_DARKGREY
#define COLOR_STATUS_BG      TFT_NAVY
#define COLOR_STATUS_TEXT    TFT_WHITE
#define COLOR_BTN_BG         TFT_DARKGREY
#define COLOR_BTN_BG_ACTIVE  TFT_RED
#define COLOR_BTN_TEXT       TFT_WHITE
#define COLOR_OVERLAY_BG     TFT_NAVY
#define COLOR_OVERLAY_BTN    TFT_BLUE
#define COLOR_OVERLAY_BTN_SEL TFT_GREEN
#define COLOR_ERROR          TFT_RED

// ---------------------------------------------------------------------------
// Terminal behavior
// ---------------------------------------------------------------------------
#define MAX_RAW_LINE_LEN      512   // guard against runaway lines with no terminator
#define HISTORY_CAPACITY      400   // number of word-wrapped display lines retained
#define NEW_DATA_FLUSH_MS     10    // redraw throttle while scrolled back / idle

// ---------------------------------------------------------------------------
// Preferences (NVS) keys
// ---------------------------------------------------------------------------
#define PREFS_NAMESPACE       "cydterm"
#define PREFS_KEY_BAUD        "baud"
#define PREFS_KEY_SESSION_NUM "session"
#define DEFAULT_BAUD_RATE     115200

// Baud rates offered on the touch-selectable baud menu.
static const uint32_t BAUD_RATE_OPTIONS[] = {
  1200, 2400, 4800, 9600, 19200, 38400, 57600,
  74880, 115200, 230400, 460800, 921600
};
#define BAUD_RATE_OPTIONS_COUNT (sizeof(BAUD_RATE_OPTIONS) / sizeof(BAUD_RATE_OPTIONS[0]))

// ---------------------------------------------------------------------------
// SD logging
// ---------------------------------------------------------------------------
#define SD_LOG_DIR             "/sessions"
#define SD_LOG_FLUSH_INTERVAL_MS 2000
