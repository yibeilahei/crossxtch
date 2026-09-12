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
bool maybeSleep(HalGPIO& gpio, const Settings& settings);
void idleDelay();

// Short power-button press (released before the deep-sleep hold) toggles clock
// mode: push ClockScreen, or fall through so ClockScreen pops on the same
// release. Idle timers: clock mode → the same overlay; true sleep → white page
// and power off. A long power hold still deep-sleeps.
bool maybeEnterClock(HalGPIO& gpio);
}  // namespace power
