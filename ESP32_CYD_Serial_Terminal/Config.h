// Config.h
// Board: ESP32-2432S028R "Cheap Yellow Display" (CYD)
// 2.8" ILI9341 320x240 TFT + XPT2046 resistive touch
//
// Central place for pin assignments, colors and tunable constants.

#pragma once

#include <Arduino.h>

// ---------------------------------------------------------------------------
// Incoming (monitored) serial line
// ---------------------------------------------------------------------------
// GPIO35 is an input-only ADC1 pin broken out on the CYD's CN1 header.
// It is not used by the TFT, touch controller or microSD slot, which makes
// it a convenient RX-only pin for sniffing a target device's TX line.
// NOTE: ESP32 GPIOs are 3.3V only. If the device being monitored uses 5V
// logic (e.g. an Arduino Uno), use a level shifter or a resistor divider
// (e.g. 10k/20k) between its TX pin and GPIO35 to avoid damaging the ESP32.
#define MONITOR_RX_PIN      35
#define MONITOR_UART        Serial2
#define MONITOR_UART_NUM    2

// ---------------------------------------------------------------------------
// TFT (ILI9341) - driven by the TFT_eSPI library.
// Pin wiring itself is configured at compile time inside the TFT_eSPI
// library's User_Setup.h (see README.md for the exact block to paste in).
// The backlight pin is controlled directly from this sketch.
// ---------------------------------------------------------------------------
#define TFT_BL_PIN           21
#define TFT_BACKLIGHT_ON     HIGH

// ---------------------------------------------------------------------------
// Touch (XPT2046) - resistive touch controller on its own bit-banged SPI
// bus (separate from the TFT's VSPI bus).
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
// microSD card slot - shares the TFT's VSPI bus (MOSI/MISO/SCLK), with its
// own chip-select line.
// ---------------------------------------------------------------------------
#define SD_CS_PIN            5
#define TFT_SCLK_PIN         14
#define TFT_MOSI_PIN         13
#define TFT_MISO_PIN         12

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
