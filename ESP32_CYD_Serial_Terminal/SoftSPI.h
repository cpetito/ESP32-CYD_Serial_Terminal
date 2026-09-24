// SoftSPI.h
// Vendored from the RandomNerdTutorials ESP32 CYD display+touch+microSD
// example (https://RandomNerdTutorials.com/esp32-cyd-display-touchscreen-microsd-card/),
// provided there for readers to copy into their own sketches. Used here
// unmodified as the bit-banged SPI backend for XPT2046_TouchscreenSOFTSPI -
// see TouchInput.h for why touch needs to be bit-banged on this board.

#ifndef _SOFTSPI_H
#define _SOFTSPI_H

#if (ARDUINO >= 100)
# include <Arduino.h>
#else
# include <WProgram.h>
#endif

#include <SPI.h>

class SoftSPI : public SPIClass {
    private:
        void wait(uint_fast8_t del);

    private:
        uint8_t _cke;
        uint8_t _ckp;
        uint8_t _delay;
        uint8_t _miso;
        uint8_t _mosi;
        uint8_t _sck;
        uint8_t _order;

    public:
        SoftSPI(uint8_t mosi, uint8_t miso, uint8_t sck);
        void begin();
        void end();
        void setBitOrder(uint8_t);
        void setDataMode(uint8_t);
        void setClockDivider(uint32_t);
        uint8_t transfer(uint8_t);
		uint16_t transfer16(uint16_t data);

};
#endif
