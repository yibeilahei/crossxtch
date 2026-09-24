#include "network/ClockSync.h"

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

// http://host[:port]/path  — plain TCP. HTTPS is not linked.
bool parseHttpUrl(const char* url, char* host, const size_t hostSize, uint16_t& port, const char*& path) {
  constexpr char kScheme[] = "http://";
  if (!url || strncmp(url, kScheme, sizeof(kScheme) - 1) != 0) {
    return false;
  }
  const char* start = url + (sizeof(kScheme) - 1);
  const char* slash = strchr(start, '/');
  const char* hostEnd = slash ? slash : start + strlen(start);
  const char* colon = nullptr;
  for (const char* c = start; c < hostEnd; ++c) {
    if (*c == ':') {
      colon = c;
    }
  }
  const size_t hostLen = static_cast<size_t>((colon ? colon : hostEnd) - start);
  if (hostLen == 0 || hostLen >= hostSize) {
    return false;
  }
  memcpy(host, start, hostLen);
  host[hostLen] = '\0';
  port = 80;
  if (colon) {
    const int parsed = atoi(colon + 1);
    if (parsed <= 0 || parsed > 65535) {
      return false;
    }
    port = static_cast<uint16_t>(parsed);
  }
  path = slash ? slash : "/";
  return true;
}

bool httpGetBody(const char* url, char* buf, const size_t bufSize) {
  if (bufSize < 2) {
    return false;
  }
  char host[96];
  uint16_t port = 80;
  const char* path = "/";
  if (!parseHttpUrl(url, host, sizeof(host), port, path)) {
    LOG_ERR("CLK", "Bad URL %s", url ? url : "");
    return false;
  }

  WiFiClient client;
  if (!client.connect(host, port, 3000)) {
    LOG_ERR("CLK", "Connect %s:%u failed", host, port);
    return false;
  }
  client.print("GET ");
  client.print(path);
  client.print(" HTTP/1.0\r\nHost: ");
  client.print(host);
  client.print("\r\nConnection: close\r\n\r\n");

  char line[160];
  int status = -1;
  bool inHeader = true;
  size_t bodyLen = 0;
  buf[0] = '\0';
  const unsigned long deadline = millis() + 4000;
  while ((client.connected() || client.available()) && millis() < deadline) {
    if (!client.available()) {
      delay(10);
      continue;
    }
    if (inHeader) {
      const size_t n = client.readBytesUntil('\n', line, sizeof(line) - 1);
      if (n == 0) {
        continue;
      }
      line[n] = '\0';
      if (line[n - 1] == '\r') {
        line[n - 1] = '\0';
      }
      if (line[0] == '\0') {
        inHeader = false;
        continue;
      }
      if (status < 0 && strncmp(line, "HTTP/", 5) == 0) {
        const char* sp = strchr(line, ' ');
        status = sp ? atoi(sp + 1) : -1;
      }
      continue;
    }
    const int c = client.read();
    if (c < 0) {
      break;
    }
    if (bodyLen + 1 >= bufSize) {
      break;
    }
    buf[bodyLen++] = static_cast<char>(c);
    buf[bodyLen] = '\0';
  }
  client.stop();
  if (status != 200 || bodyLen == 0) {
    LOG_ERR("CLK", "GET %s -> %d (%u bytes)", url, status, static_cast<unsigned>(bodyLen));
    return false;
  }
  return true;
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
  const bool needTimezone = !settings.clockHasBeenSynced;
  if (halClock.isAvailable()) {
    bool ntpDue = true;
    Rtc::DateTime now{};
    if (halClock.nowUtc(now) && settings.ntpSyncYear >= 2020 && settings.ntpSyncMonth >= 1 &&
        settings.ntpSyncMonth <= 12) {
      const int last = static_cast<int>(settings.ntpSyncYear) * 12 + settings.ntpSyncMonth;
      const int cur = static_cast<int>(now.year) * 12 + now.month;
      ntpDue = cur > last;
    }
    if (ntpDue) {
      LOG_INF("CLK", "Monthly NTP sync");
      if (halClock.syncFromNTP()) {
        Rtc::DateTime after{};
        if (halClock.nowUtc(after)) {
          settings.ntpSyncYear = after.year;
          settings.ntpSyncMonth = after.month;
        }
        if (!settings.clockHasBeenSynced) {
          settings.clockHasBeenSynced = 1;
        }
        dirty = true;
      }
    }
  }

  // Timezone HTTP is slow (~2s) and rarely changes. First connect looks it
  // up; later connects (or a manual set from the file-manager page) only
  // correct RTC drift via NTP.
  if (needTimezone) {
    uint8_t offsetQ = 0;
    if (fetchTimezoneOffset(offsetQ) && offsetQ != settings.clockUtcOffsetQ) {
      settings.clockUtcOffsetQ = offsetQ;
      dirty = true;
    }
  }
  if (dirty) {
    settings.save();
  }
}
