// TouchInput.cpp
#include "TouchInput.h"

// XPT2046 control bytes (datasheet Table 1): differential mode, 12-bit
// conversion, power-down between conversions so IRQ re-arms after each
// read. These are the same control bytes essentially every XPT2046
// Arduino library uses, so raw values here should match what
// TOUCH_RAW_X/Y_MIN/MAX were calibrated against on a hardware-SPI reading.
static const uint8_t XPT_CMD_X = 0xD0;
static const uint8_t XPT_CMD_Y = 0x90;

void TouchInput::begin(int16_t screenW, int16_t screenH) {
  screenW_ = screenW;
  screenH_ = screenH;

  pinMode(TOUCH_CS_PIN, OUTPUT);
  pinMode(TOUCH_CLK_PIN, OUTPUT);
  pinMode(TOUCH_MOSI_PIN, OUTPUT);
  pinMode(TOUCH_MISO_PIN, INPUT);
  pinMode(TOUCH_IRQ_PIN, INPUT);

  digitalWrite(TOUCH_CS_PIN, HIGH);
  digitalWrite(TOUCH_CLK_PIN, LOW);
}

// One SPI MODE 0 bit: MOSI is set up while the clock is low, and both
// sides sample on the rising edge.
static inline uint8_t xptBit(uint8_t outBit) {
  digitalWrite(TOUCH_MOSI_PIN, outBit);
  digitalWrite(TOUCH_CLK_PIN, HIGH);
  uint8_t inBit = digitalRead(TOUCH_MISO_PIN);
  digitalWrite(TOUCH_CLK_PIN, LOW);
  return inBit;
}

// Sends an 8-bit command, then clocks in the 16-bit response that follows
// it and extracts the 12-bit conversion result: a leading null bit, 12
// data bits (MSB first), then 3 trailing don't-care bits.
static uint16_t xptRead(uint8_t command) {
  for (int8_t i = 7; i >= 0; i--) {
    xptBit((command >> i) & 0x01);
  }
  uint16_t result = 0;
  for (int8_t i = 0; i < 16; i++) {
    result = (uint16_t)(result << 1) | xptBit(0);
  }
  return (result >> 3) & 0x0FFF;
}

bool TouchInput::getPoint(int16_t &x, int16_t &y) {
  // The XPT2046 pulls IRQ low while pressed; skip the bit-banged read
  // entirely when it's not, both to save time and to avoid clocking out
  // noise from an unpressed/floating conversion.
  if (digitalRead(TOUCH_IRQ_PIN) == HIGH) return false;

  digitalWrite(TOUCH_CS_PIN, LOW);
  uint16_t rawXValue = xptRead(XPT_CMD_X);
  uint16_t rawYValue = xptRead(XPT_CMD_Y);
  digitalWrite(TOUCH_CS_PIN, HIGH);

  if (digitalRead(TOUCH_IRQ_PIN) == HIGH) return false; // released mid-read

  // Which raw axis feeds the screen's X vs Y, and its calibration range -
  // see the TOUCH_SWAP_XY comment in Config.h for why this board needs it.
#if TOUCH_SWAP_XY
  long rawX = rawYValue, rawXMin = TOUCH_RAW_Y_MIN, rawXMax = TOUCH_RAW_Y_MAX;
  long rawY = rawXValue, rawYMin = TOUCH_RAW_X_MIN, rawYMax = TOUCH_RAW_X_MAX;
#else
  long rawX = rawXValue, rawXMin = TOUCH_RAW_X_MIN, rawXMax = TOUCH_RAW_X_MAX;
  long rawY = rawYValue, rawYMin = TOUCH_RAW_Y_MIN, rawYMax = TOUCH_RAW_Y_MAX;
#endif

#if TOUCH_INVERT_X
  long mx = map(rawX, rawXMin, rawXMax, screenW_, 0);
#else
  long mx = map(rawX, rawXMin, rawXMax, 0, screenW_);
#endif
#if TOUCH_INVERT_Y
  long my = map(rawY, rawYMin, rawYMax, screenH_, 0);
#else
  long my = map(rawY, rawYMin, rawYMax, 0, screenH_);
#endif

  mx = constrain(mx, 0, screenW_ - 1);
  my = constrain(my, 0, screenH_ - 1);

  x = (int16_t)mx;
  y = (int16_t)my;

#if TOUCH_DEBUG_SERIAL
  Serial.printf("touch raw=(%u,%u) mapped=(%d,%d)\n", rawXValue, rawYValue, x, y);
#endif

  return true;
}
