// SDLogger.cpp
#include "SDLogger.h"
#include <SPI.h>

bool SDLogger::mount() {
  if (mounted_) return true;

  // TFT_eSPI already owns the VSPI bus (SCLK/MOSI/MISO); make sure those
  // pins are explicitly (re)configured with the SD card's CS line before
  // handing the bus to the SD library.
  SPI.begin(TFT_SCLK_PIN, TFT_MISO_PIN, TFT_MOSI_PIN, SD_CS_PIN);

  if (!SD.begin(SD_CS_PIN, SPI)) {
    mounted_ = false;
    return false;
  }

  if (SD.cardType() == CARD_NONE) {
    mounted_ = false;
    return false;
  }

  if (!SD.exists(SD_LOG_DIR)) {
    SD.mkdir(SD_LOG_DIR);
  }

  mounted_ = true;
  return true;
}

bool SDLogger::startSession(Settings &settings, String &outFileName) {
  if (!mount()) {
    return false;
  }

  uint32_t n = settings.nextSessionNumber();
  char nameBuf[48];
  snprintf(nameBuf, sizeof(nameBuf), "%s/session_%04u.log", SD_LOG_DIR, (unsigned)n);
  currentFileName_ = String(nameBuf);

  file_ = SD.open(currentFileName_, FILE_WRITE);
  if (!file_) {
    return false;
  }

  file_.printf("# CYD Serial Terminal session %u started at millis()=%lu\n",
               (unsigned)n, millis());
  file_.flush();
  lastFlushMs_ = millis();
  outFileName = currentFileName_;
  return true;
}

void SDLogger::stopSession() {
  if (file_) {
    file_.flush();
    file_.close();
  }
}

void SDLogger::writeLine(const String &line) {
  if (!file_) return;
  file_.print(line);
  file_.print('\n');

  unsigned long now = millis();
  if (now - lastFlushMs_ >= SD_LOG_FLUSH_INTERVAL_MS) {
    file_.flush();
    lastFlushMs_ = now;
  }
}

String SDLogger::statusText() const {
  if (file_) return "REC";
  if (mounted_) return "SD OK";
  return "NO SD";
}
