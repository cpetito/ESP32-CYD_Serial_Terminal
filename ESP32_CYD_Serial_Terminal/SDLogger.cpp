// SDLogger.cpp
#include "SDLogger.h"
#include <SPI.h>

bool SDLogger::mount() {
  if (mounted_) return true;

  // SD.end() first guarantees a clean re-init if a prior mount attempt
  // failed partway through.
  SD.end();
  SPI.begin(SD_SCLK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);

  // A lower SPI clock than the SD library's 4MHz default trades a little
  // speed for reliability on a marginal card or long/shared wiring - well
  // worth it for a text logger that isn't throughput bound.
  if (!SD.begin(SD_CS_PIN, SPI, 4000000)) {
    Serial.println(F("SD: SD.begin() failed - check card is inserted/seated, "
                      "formatted FAT16/FAT32, and that SD_CS/SCLK/MOSI/MISO_PIN "
                      "in Config.h match your board revision."));
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

  if (SD.exists(SD_LOG_DIR)) {
    logDir_ = SD_LOG_DIR;
  } else if (SD.mkdir(SD_LOG_DIR)) {
    logDir_ = SD_LOG_DIR;
  } else {
    // A card that mounts (correct size/type reported) but can't create a
    // directory almost always means its filesystem isn't FAT16/FAT32 -
    // most commonly a 16GB+ card preformatted as exFAT, which this SD
    // library can't write to at all. Fall back to the root directory
    // rather than hard-failing every recording: if writes there also fail
    // (see startSession()), that confirms it's a filesystem-type issue,
    // not just this one subdirectory - reformat the card as FAT32.
    Serial.println(F("SD: mkdir(" SD_LOG_DIR ") failed - falling back to the "
                      "card's root directory for session files. If writes "
                      "there fail too, the card's filesystem is likely not "
                      "FAT16/FAT32 (e.g. exFAT) - reformat it as FAT32."));
    logDir_ = "";
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
  if (logDir_.length() > 0) {
    snprintf(nameBuf, sizeof(nameBuf), "%s/session_%04u.log", logDir_.c_str(), (unsigned)n);
  } else {
    snprintf(nameBuf, sizeof(nameBuf), "/session_%04u.log", (unsigned)n);
  }
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
