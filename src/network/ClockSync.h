#pragma once

// NTP (every call) + timezone HTTP (only until clockHasBeenSynced).
// Call while STA is connected; safe after the file-transfer server is up.
namespace ClockSync {
void onWifiConnected();
}
