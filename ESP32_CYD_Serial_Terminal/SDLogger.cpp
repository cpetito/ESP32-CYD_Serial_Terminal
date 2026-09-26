// SDLogger.cpp
#include "SDLogger.h"

bool SDLogger::mount() {
  // Deliberately no "if (mounted_) return true" short-circuit: this is
  // only ever called at boot and once per REC tap, so it's cheap enough
  // to always re-verify the card is genuinely still there, rather than
  // trust a flag that would otherwise go stale the moment a card is
  // pulled out after a successful mount (REC would keep reporting
  // "SD OK" indefinitely, since nothing would ever re-check).

  // sdSPI_ is a dedicated VSPI peripheral, exclusively the SD card's own -
  // see the comment on sdSPI_ in SDLogger.h for why VSPI specifically (the
  // TFT's User_Setup.h moves it onto HSPI). SD.end() first guarantees a
  // clean re-init, including a re-check of a card that's since been
  // removed (or replaced) rather than reusing whatever was true before.
  SD.end();
  sdSPI_.begin(SD_SCLK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);

  // See SD_SPI_CLOCK_HZ in Config.h for why this isn't a conservative
  // low value.
  if (!SD.begin(SD_CS_PIN, sdSPI_, SD_SPI_CLOCK_HZ)) {
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
    // directory can mean its filesystem isn't FAT16/FAT32 (e.g. exFAT), but
    // if writes fail everywhere - including at the root, see the self-test
    // below - a filesystem-verified-good card points instead at power or
    // wiring during the write itself. Fall back to the root directory
    // rather than hard-failing every recording either way.
    Serial.println(F("SD: mkdir(" SD_LOG_DIR ") failed - falling back to the "
                      "card's root directory for session files."));
    logDir_ = "";
  }

  mounted_ = true;
  runWriteSelfTest("SD mount");
  return true;
}

void SDLogger::runWriteSelfTest(const char *label) {
  const char *testPath = "/cydtest.tmp";
  SD.remove(testPath); // clean slate; ignore failure if it doesn't exist yet

  File f = SD.open(testPath, FILE_WRITE);
  if (!f) {
    Serial.printf("%s: write self-test FAILED to open a test file at the "
                  "card's root.\n", label);
    return;
  }

  size_t written = f.print("cyd write self-test");
  f.flush();
  f.close();
  if (written == 0) {
    Serial.printf("%s: write self-test opened the test file but wrote 0 "
                  "bytes.\n", label);
    SD.remove(testPath);
    return;
  }

  File rf = SD.open(testPath, FILE_READ);
  bool ok = rf && rf.available() > 0;
  if (rf) rf.close();
  SD.remove(testPath);

  Serial.printf("%s: write self-test %s\n", label,
                ok ? "passed" : "wrote a file but could not read it back");
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
