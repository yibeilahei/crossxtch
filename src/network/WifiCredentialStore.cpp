#include "network/WifiCredentialStore.h"

#include <HalStorage.h>
#include <Logging.h>

#include <cstring>

WifiCredentialStore wifiCredentials;

void WifiCredentialStore::load() {
  WifiCredentialStore loaded{};
  HalFile f;
  if (!Storage.openFileForRead("WIFI", kPath, f)) {
    LOG_INF("WIFI", "No saved networks");
    return;
  }
  const size_t n = static_cast<size_t>(f.read(reinterpret_cast<uint8_t*>(&loaded), sizeof(loaded)));
  if (n != sizeof(loaded) || loaded.magic != MAGIC || loaded.version != VERSION) {
    LOG_ERR("WIFI", "Ignoring wifi file (n=%u magic=0x%08lX ver=%u)", static_cast<unsigned>(n),
            static_cast<unsigned long>(loaded.magic), loaded.version);
    return;
  }
  *this = loaded;
  for (auto& slot : slots) {
    slot.ssid[sizeof(slot.ssid) - 1] = '\0';
    slot.password[sizeof(slot.password) - 1] = '\0';
  }
  LOG_INF("WIFI", "Loaded saved networks");
}

void WifiCredentialStore::save() const {
  Storage.ensureDirectoryExists("/.crossxtch");
  HalFile f;
  if (!Storage.openFileForWrite("WIFI", kPath, f)) {
    LOG_ERR("WIFI", "Could not write %s", kPath);
    return;
  }
  const size_t n = f.write(this, sizeof(*this));
  if (n != sizeof(*this)) {
    LOG_ERR("WIFI", "Short wifi write (%u of %u)", static_cast<unsigned>(n), static_cast<unsigned>(sizeof(*this)));
  }
}

const WifiCredentialStore::Entry* WifiCredentialStore::find(const char* ssid) const {
  if (!ssid) {
    return nullptr;
  }
  for (const auto& slot : slots) {
    if (slot.used && strncmp(slot.ssid, ssid, sizeof(slot.ssid)) == 0) {
      return &slot;
    }
  }
  return nullptr;
}

void WifiCredentialStore::addOrUpdate(const char* ssid, const char* password) {
  if (!ssid || ssid[0] == '\0') {
    return;
  }

  for (auto& slot : slots) {
    if (slot.used && strncmp(slot.ssid, ssid, sizeof(slot.ssid)) == 0) {
      snprintf(slot.password, sizeof(slot.password), "%s", password ? password : "");
      save();
      return;
    }
  }

  for (auto& slot : slots) {
    if (!slot.used) {
      snprintf(slot.ssid, sizeof(slot.ssid), "%s", ssid);
      snprintf(slot.password, sizeof(slot.password), "%s", password ? password : "");
      slot.used = true;
      save();
      return;
    }
  }

  // Full: evict round-robin so one bad network can't permanently block new saves.
  Entry& victim = slots[nextEvict % kMaxNetworks];
  snprintf(victim.ssid, sizeof(victim.ssid), "%s", ssid);
  snprintf(victim.password, sizeof(victim.password), "%s", password ? password : "");
  victim.used = true;
  nextEvict = (nextEvict + 1) % kMaxNetworks;
  save();
}
