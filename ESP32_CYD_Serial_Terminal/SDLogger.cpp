// SDLogger.cpp
#include "SDLogger.h"
#include <SPI.h>

bool SDLogger::mount() {
  if (mounted_) return true;

  // TFT_eSPI already owns the VSPI bus (SCLK/MOSI/MISO); make sure those
  // pins are explicitly (re)configured with the SD card's CS line before
  // handing the bus to the SD library. SD.end() first guarantees a clean
  // re-init if a prior mount attempt failed partway through.
  SD.end();
  SPI.begin(TFT_SCLK_PIN, TFT_MISO_PIN, TFT_MOSI_PIN, SD_CS_PIN);

  // A lower SPI clock than the SD library's 4MHz default trades a little
  // speed for reliability on the CYD's shared, sometimes marginal SD
  // wiring - well worth it for a text logger that isn't throughput bound.
  if (!SD.begin(SD_CS_PIN, SPI, 4000000)) {
    Serial.println(F("SD: SD.begin() failed - check card is inserted/seated, "
                      "formatted FAT16/FAT32, and that SD_CS_PIN in Config.h "
                      "matches your board revision."));
    mounted_ = false;
    return false;
  }

  uint8_t cardType = SD.cardType();
  if (cardType == CARD_NONE) {
    Serial.println(F("SD: SD.begin() succeeded but no card was detected "
                      "(cardType() == CARD_NONE) - likely not fully seated."));
    mounted_ = false;
    return false;
  }

  Serial.printf("SD: mounted OK (cardType=%u, %llu MB)\n",
                (unsigned)cardType,
                (unsigned long long)(SD.cardSize() / (1024ULL * 1024ULL)));

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
    Serial.printf("SD: failed to open %s for writing\n", currentFileName_.c_str());
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
