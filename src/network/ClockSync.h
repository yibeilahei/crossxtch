#pragma once

// NTP at most once per calendar month; timezone HTTP until first success.
// Call while STA is connected and before the file-transfer server starts.
namespace ClockSync {
void onWifiConnected();
}
