#include "network/WifiManager.h"

#include <Logging.h>
#include <WiFi.h>

#include <cstdio>

WifiManager wifiManager;

void WifiManager::startScan() {
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  LOG_INF("WIFI", "Scan starting");
  WiFi.scanNetworks(true /* async */);
}

bool WifiManager::scanComplete(std::vector<Network>& out) {
  const int n = WiFi.scanComplete();
  if (n == WIFI_SCAN_RUNNING) {
    return false;
  }
  if (n == WIFI_SCAN_FAILED) {
    LOG_ERR("WIFI", "Scan failed");
    out.clear();
    return true;
  }

  out.clear();
  out.reserve(static_cast<size_t>(n));
  for (int i = 0; i < n; ++i) {
    Network net;
    net.ssid = WiFi.SSID(i).c_str();
    net.rssi = WiFi.RSSI(i);
    net.encrypted = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
    // Skip duplicate SSIDs (multiple APs on the same network), keeping the strongest.
    bool merged = false;
    for (auto& existing : out) {
      if (existing.ssid == net.ssid) {
        if (net.rssi > existing.rssi) {
          existing.rssi = net.rssi;
          existing.encrypted = net.encrypted;
        }
        merged = true;
        break;
      }
    }
    if (!merged && !net.ssid.empty()) {
      out.push_back(net);
    }
  }
  WiFi.scanDelete();
  LOG_INF("WIFI", "Scan complete: %u network(s)", static_cast<unsigned>(out.size()));
  return true;
}

void WifiManager::connect(const char* ssid, const char* password, const unsigned long timeoutMs) {
  LOG_INF("WIFI", "Connecting to '%s'", ssid ? ssid : "");
  WiFi.persistent(false);  // Credentials live in WifiCredentialStore, not NVS.
  WiFi.setAutoReconnect(false);
  WiFi.mode(WIFI_STA);
  // Abort any prior connect attempt first. Without this, retrying right
  // after a timeout (the underlying esp-idf driver may still be mid-connect)
  // makes WiFi.begin() fail immediately with "sta is connecting, cannot set
  // config", which then times out again — an unrecoverable retry loop.
  WiFi.disconnect();
  if (password && password[0] != '\0') {
    WiFi.begin(ssid, password);
  } else {
    WiFi.begin(ssid);
  }
  connectStartMs = millis();
  connectTimeoutMs = timeoutMs == 0 ? kConnectTimeoutMs : timeoutMs;
}

void WifiManager::abortConnect() { WiFi.disconnect(); }

WifiManager::ConnectState WifiManager::pollConnect() {
  const wl_status_t status = WiFi.status();
  if (status == WL_CONNECTED) {
    // Disable STA modem-sleep: its periodic radio naps add real latency to
    // every received packet and are a well-known ESP32 throughput killer,
    // especially for sustained transfers like ours.
    WiFi.setSleep(false);
    LOG_INF("WIFI", "Connected, ip=%s", WiFi.localIP().toString().c_str());
    return ConnectState::Connected;
  }
  // Wrong password typically lands here instead of waiting out the timeout.
  if (status == WL_CONNECT_FAILED || status == WL_NO_SSID_AVAIL) {
    LOG_ERR("WIFI", "Connect failed (status=%d)", static_cast<int>(status));
    WiFi.disconnect();
    return ConnectState::Failed;
  }
  if (millis() - connectStartMs > connectTimeoutMs) {
    LOG_ERR("WIFI", "Connect timed out (status=%d)", static_cast<int>(status));
    // Abort the stuck attempt so a subsequent connect() doesn't immediately
    // fail with "sta is connecting, cannot set config".
    WiFi.disconnect();
    return ConnectState::Failed;
  }
  return ConnectState::Connecting;
}

void WifiManager::disconnect() {
  WiFi.disconnect(true /* wifioff */);
  WiFi.mode(WIFI_OFF);
}

bool WifiManager::isConnected() const { return WiFi.status() == WL_CONNECTED; }

std::string WifiManager::localIP() const { return std::string(WiFi.localIP().toString().c_str()); }

std::string WifiManager::ssid() const { return std::string(WiFi.SSID().c_str()); }
