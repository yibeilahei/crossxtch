#pragma once

#include <cstdint>

struct Settings {
  static constexpr uint32_t MAGIC = 0x48585443;  // "HXTC"
  static constexpr uint16_t VERSION = 6;
  static constexpr uint16_t kMinVersion = 3;
  static constexpr const char* kDir = "/.crossxtch";
  static constexpr const char* kPath = "/.crossxtch/settings.bin";

  uint32_t magic = MAGIC;
  uint16_t version = VERSION;
  uint8_t sleepTimeoutSeconds = 60;  // light sleep (gyro lock); 0 = none, else 30/45/60
  uint8_t refreshEveryNPages = 5;
  uint8_t nightMode = 0;
  uint8_t tiltPageTurn = 0;  // 0 = off, 1 = on (flick either direction pages forward, X3 only)
  char lastBookPath[200]{};
  uint8_t trueSleepMinutes = 0;  // deep sleep; 0 = never, else 10/20/30
  uint8_t clockHasBeenSynced = 0;  // set after a successful NTP write to the RTC
  uint8_t clockUtcOffsetQ = 48;    // 48 = UTC+0; 15-minute steps, 0 = UTC-12, 104 = UTC+14

  void load();
  void save() const;
  unsigned long lightSleepTimeoutMs() const;
  unsigned long trueSleepTimeoutMs() const;
};

extern Settings settings;
