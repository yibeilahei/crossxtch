#include "network/ClockSync.h"

#include <HTTPClient.h>
#include <HalClock.h>
#include <Logging.h>
#include <WiFi.h>
#include <WiFiClient.h>

#include <cstdlib>
#include <cstring>

#include "core/Settings.h"

namespace {
bool secondsToBiased(const int seconds, uint8_t& out) {
  const int quarters = seconds >= 0 ? (seconds + 450) / 900 : (seconds - 450) / 900;
  const int biased = quarters + 48;
  if (biased < 0 || biased > 104) {
    return false;
  }
  out = static_cast<uint8_t>(biased);
  return true;
}

bool parseOffsetJson(const char* body, uint8_t& out) {
  if (!body) {
    return false;
  }
  const char* p = strstr(body, "\"offset\"");
  if (p) {
    p = strchr(p, ':');
    if (p) {
      return secondsToBiased(atoi(p + 1), out);
    }
  }
  p = strstr(body, "\"utc_offset\"");
  if (!p) {
    return false;
  }
  p = strchr(p + 12, ':');
  if (!p) {
    return false;
  }
  ++p;
  while (*p == ' ' || *p == '"') {
    ++p;
  }
  int sign = 1;
  if (*p == '+') {
    ++p;
  } else if (*p == '-') {
    sign = -1;
    ++p;
  }
  const int hours = atoi(p);
  const char* colon = strchr(p, ':');
  const int mins = colon ? atoi(colon + 1) : 0;
  return secondsToBiased(sign * (hours * 3600 + mins * 60), out);
}

bool httpGetBody(const char* url, char* buf, const size_t bufSize) {
  WiFiClient client;
  HTTPClient http;
  http.setConnectTimeout(3000);
  http.setTimeout(4000);
  http.setReuse(false);
  if (!http.begin(client, url)) {
    return false;
  }
  const int code = http.GET();
  bool ok = false;
  if (code == HTTP_CODE_OK) {
    const String body = http.getString();
    if (!body.isEmpty() && static_cast<size_t>(body.length()) + 1 <= bufSize) {
      memcpy(buf, body.c_str(), static_cast<size_t>(body.length()) + 1);
      ok = true;
    }
  } else {
    LOG_ERR("CLK", "GET %s -> %d", url, code);
  }
  http.end();
  return ok;
}

bool fetchTimezoneOffset(uint8_t& out) {
  char body[512];
  constexpr const char* kUrls[] = {"http://ip-api.com/json/?fields=status,offset", "http://worldtimeapi.org/api/ip"};
  for (const char* url : kUrls) {
    if (!httpGetBody(url, body, sizeof(body))) {
      continue;
    }
    if (parseOffsetJson(body, out)) {
      LOG_INF("CLK", "Timezone offset q=%u from %s", out, url);
      return true;
    }
  }
  LOG_ERR("CLK", "Timezone lookup failed");
  return false;
}
}  // namespace

void ClockSync::onWifiConnected() {
  if (WiFi.status() != WL_CONNECTED) {
    return;
  }

  bool dirty = false;
  if (halClock.isAvailable()) {
    LOG_INF("CLK", "Syncing clock from NTP");
    if (halClock.syncFromNTP() && !settings.clockHasBeenSynced) {
      settings.clockHasBeenSynced = 1;
      dirty = true;
    }
  }

  uint8_t offsetQ = 0;
  if (fetchTimezoneOffset(offsetQ) && offsetQ != settings.clockUtcOffsetQ) {
    settings.clockUtcOffsetQ = offsetQ;
    dirty = true;
  }
  if (dirty) {
    settings.save();
  }
}
