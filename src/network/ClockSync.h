#pragma once

// NTP + IP-based timezone lookup. Call while STA is connected.
namespace ClockSync {
void onWifiConnected();
}
