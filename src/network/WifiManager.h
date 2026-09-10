#pragma once

#include <cstdint>
#include <string>
#include <vector>

// Thin wrapper over Arduino WiFi.h for STA-only join. No AP/hotspot mode.
class WifiManager {
 public:
  struct Network {
    std::string ssid;
    int32_t rssi;
    bool encrypted;
  };

  enum class ConnectState { Idle, Connecting, Connected, Failed };

  static constexpr unsigned long kConnectTimeoutMs = 15000;
  static constexpr unsigned long kAutoConnectTimeoutMs = 8000;

  // Starts an async scan. Call scanComplete() to poll for results.
  void startScan();
  // Returns true once the scan has finished (success or failure) and fills `out`.
  // `out` is left unchanged while the scan is still running.
  bool scanComplete(std::vector<Network>& out);

  void connect(const char* ssid, const char* password, unsigned long timeoutMs = kConnectTimeoutMs);
  // Stop an in-progress attempt without powering the radio off.
  void abortConnect();
  // Advances connection state; call every loop() while Connecting.
  ConnectState pollConnect();
  void disconnect();

  bool isConnected() const;
  std::string localIP() const;
  std::string ssid() const;

 private:
  unsigned long connectStartMs = 0;
  unsigned long connectTimeoutMs = kConnectTimeoutMs;
};

extern WifiManager wifiManager;
