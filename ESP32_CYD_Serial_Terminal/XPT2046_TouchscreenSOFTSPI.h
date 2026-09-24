// XPT2046_TouchscreenSOFTSPI.h
// Vendored from the RandomNerdTutorials ESP32 CYD display+touch+microSD
// example (https://RandomNerdTutorials.com/esp32-cyd-display-touchscreen-microsd-card/),
// a SoftSPI-based fork of Paul Stoffregen's XPT2046_Touchscreen library,
// provided there for readers to copy into their own sketches. Used here
// unmodified - see TouchInput.h for why touch needs to be bit-banged on
// this board rather than sharing a hardware SPI peripheral with the TFT
// or microSD card.

#ifndef _XPT2046_Touchscreen_h_
#define _XPT2046_Touchscreen_h_

#include "Arduino.h"
#include <SPI.h>
#include <SoftSPI.h> // i add for SoftSPI

#if ARDUINO < 10600
#error "Arduino 1.6.0 or later (SPI library) is required"
#endif



class TS_Point {
public:
	TS_Point(void) : x(0), y(0), z(0) {}
	TS_Point(int16_t x, int16_t y, int16_t z) : x(x), y(y), z(z) {}
	bool operator==(TS_Point p) { return ((p.x == x) && (p.y == y) && (p.z == z)); }
	bool operator!=(TS_Point p) { return ((p.x != x) || (p.y != y) || (p.z != z)); }
	int16_t x, y, z;
};

class XPT2046_TouchscreenSOFTSPI {
public:
	constexpr XPT2046_TouchscreenSOFTSPI(uint8_t cspin, uint8_t tirq=255)
		: csPin(cspin), tirqPin(tirq) { }
	bool begin(SoftSPI* touchscreenSPI);
	TS_Point getPoint(SoftSPI* touchscreenSPI);
	bool tirqTouched();
	bool touched(SoftSPI *touchscreenSPI);
	void readData(SoftSPI *touchscreenSPI,uint16_t *x, uint16_t *y, uint8_t *z);
	bool bufferEmpty();
	uint8_t bufferSize() { return 1; }
	void setRotation(uint8_t n) { rotation = n % 4; }
// protected:
	volatile bool isrWake=true;

private:
	void update(SoftSPI *touchscreenSPI);
	uint8_t csPin, tirqPin, rotation=1;
	int16_t xraw=0, yraw=0, zraw=0;
	uint32_t msraw=0x80000000;
};

#ifndef ISR_PREFIX
  #if defined(ESP8266)
    #define ISR_PREFIX ICACHE_RAM_ATTR
  #elif defined(ESP32)
    // TODO: should this also be ICACHE_RAM_ATTR ??
    #define ISR_PREFIX IRAM_ATTR
  #else
    #define ISR_PREFIX
  #endif
#endif

#endif
