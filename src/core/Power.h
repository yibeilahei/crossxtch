#pragma once

class HalGPIO;
struct Settings;

// Wake, idle timeout, and power-button hold-to-sleep. Owned by the main loop,
// not by individual screens.
namespace power {
void noteWakeHold();
bool isWakeReleasePending();
bool consumeWakeRelease(HalGPIO& gpio);
void noteUserActivity(HalGPIO& gpio);
// Latch a long power hold sampled outside the main loop (blocking panel BUSY
// wait). maybeSleep() consumes it so a hold that ends before the refresh
// returns still powers the device off.
void pollForHold(HalGPIO& gpio);
bool maybeSleep(HalGPIO& gpio, const Settings& settings);
void idleDelay();

// Short power-button press (released before the deep-sleep hold) toggles a
// gyroscope lock so picking up the device cannot turn pages. Any other key
// press clears the lock and is still handled normally. Idle timer: gyro
// auto-off → the same lock. A long power hold still deep-sleeps.
bool tiltLocked();
bool maybeToggleTiltLock(HalGPIO& gpio);
// 8pt top-left marker on the current reader page. Call after a page blit
// if tiltLocked() so the indicator is present when opening a book already locked.
void paintGyroOffMarker();
}  // namespace power
