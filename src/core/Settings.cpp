#include "core/Settings.h"

#include <HalStorage.h>
#include <Logging.h>

#include <cstring>

Settings settings;

void Settings::load() {
  Settings loaded{};
  HalFile f;
  if (!Storage.openFileForRead("SET", kPath, f)) {
    LOG_INF("SET", "No settings file, using defaults");
    return;
  }
  const size_t n = static_cast<size_t>(f.read(reinterpret_cast<uint8_t*>(&loaded), sizeof(loaded)));
  if (n < 10 || loaded.magic != MAGIC || loaded.version < kMinVersion || loaded.version > VERSION) {
    LOG_ERR("SET", "Ignoring settings file (n=%u magic=0x%08lX ver=%u, need %u/0x%08lX/%u-%u)",
            static_cast<unsigned>(n), static_cast<unsigned long>(loaded.magic), loaded.version,
            static_cast<unsigned>(sizeof(loaded)), static_cast<unsigned long>(MAGIC), kMinVersion, VERSION);
    return;
  }
  if (loaded.version < 4) {
    loaded.trueSleepMinutes = 0;
  }
  if (loaded.version < 5) {
    loaded.clockHasBeenSynced = 0;
  }
  if (loaded.version < 6) {
    loaded.clockUtcOffsetQ = 48;
  }
  loaded.version = VERSION;
  *this = loaded;
  lastBookPath[sizeof(lastBookPath) - 1] = '\0';
  if (clockModeSeconds != 0 && clockModeSeconds != 30 && clockModeSeconds != 45 &&
      clockModeSeconds != 60) {
    clockModeSeconds = 60;
  }
  if (trueSleepMinutes != kSleepNone && trueSleepMinutes != kSleep15Min && trueSleepMinutes != kSleep1Hour &&
      trueSleepMinutes != kSleep6Hours) {
    trueSleepMinutes = kSleep15Min;
  }
  if (clockUtcOffsetQ > 104) {
    clockUtcOffsetQ = 48;
  }
  LOG_INF("SET", "Loaded clockMode=%u sec sleep=%u refresh=%u night=%u tilt=%u clock=%u tzq=%u last='%s'",
          clockModeSeconds, trueSleepMinutes, refreshEveryNPages, nightMode, tiltPageTurn, clockHasBeenSynced,
          clockUtcOffsetQ, lastBookPath);
}

void Settings::save() const {
  Storage.ensureDirectoryExists(kDir);
  HalFile f;
  if (!Storage.openFileForWrite("SET", kPath, f)) {
    LOG_ERR("SET", "Could not write %s", kPath);
    return;
  }
  const size_t n = f.write(this, sizeof(*this));
  if (n != sizeof(*this)) {
    LOG_ERR("SET", "Short settings write (%u of %u)", static_cast<unsigned>(n), static_cast<unsigned>(sizeof(*this)));
    return;
  }
  LOG_DBG("SET", "Saved clockMode=%u sleep=%u refresh=%u night=%u tilt=%u last='%s'", clockModeSeconds,
          trueSleepMinutes, refreshEveryNPages, nightMode, tiltPageTurn, lastBookPath);
}

namespace {
unsigned long minutesToMs(const uint8_t minutes) {
  if (minutes == 0) {
    return 0;
  }
  return static_cast<unsigned long>(minutes) * 60UL * 1000UL;
}
}  // namespace

unsigned long Settings::clockModeTimeoutMs() const {
  if (clockModeSeconds == 0) {
    return 0;
  }
  return static_cast<unsigned long>(clockModeSeconds) * 1000UL;
}

unsigned long Settings::trueSleepTimeoutMs() const {
  if (trueSleepMinutes == kSleepNone) {
    return 0;
  }
  if (trueSleepMinutes == kSleep6Hours) {
    return 6UL * 60UL * 60UL * 1000UL;
  }
  return minutesToMs(trueSleepMinutes);
}
