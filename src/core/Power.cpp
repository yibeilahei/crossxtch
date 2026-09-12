#include "core/Power.h"

#include <Arduino.h>
#include <HalDisplay.h>
#include <HalGPIO.h>
#include <HalPowerManager.h>
#include <HalTiltSensor.h>
#include <Logging.h>

#include "core/ScreenManager.h"
#include "core/Settings.h"

namespace {
unsigned long allowSleepAt = 0;
unsigned long lastActivity = 0;
bool wakePowerReleasePending = false;
bool powerReleasedSinceWake = false;
// Release before this held duration counts as a short press (toggles clock
// mode); at or beyond it, the button is treated as the deep-sleep hold in
// maybeSleep() instead.
constexpr unsigned long kShortPressMs = 800;

void paintDeepSleepWhite() {
  // Physical white regardless of night mode: inversion is applied on the
  // way to the panel, so turn it off before filling 0xFF.
  display.setInverted(false);
  display.clearScreen(0xFF);
  display.displayBuffer(HalDisplay::FULL_REFRESH);
}

void enterDeepSleep(HalGPIO& gpio) {
  paintDeepSleepWhite();
  halTiltSensor.deepSleep();
  display.deepSleep();
  powerManager.startDeepSleep(gpio);
}
}  // namespace

void power::noteWakeHold() {
  wakePowerReleasePending = true;
  allowSleepAt = millis() + 2000;
  lastActivity = millis();
  LOG_DBG("SLP", "Wake hold, sleep allowed in 2s");
}

bool power::isWakeReleasePending() { return wakePowerReleasePending; }

bool power::consumeWakeRelease(HalGPIO& gpio) {
  if (wakePowerReleasePending && !gpio.isPressed(HalGPIO::BTN_POWER)) {
    wakePowerReleasePending = false;
    return true;
  }
  return false;
}

void power::noteUserActivity(HalGPIO& gpio) {
  if (gpio.wasAnyPressed() || gpio.wasAnyReleased() || halTiltSensor.hadActivity()) {
    lastActivity = millis();
    powerManager.setPowerSaving(false);
  }
  if (!gpio.isPressed(HalGPIO::BTN_POWER)) {
    powerReleasedSinceWake = true;
  }
}

bool power::maybeSleep(HalGPIO& gpio, const Settings& settings) {
  if (screenManager.blocksSleep()) {
    return false;
  }
  if (lastActivity == 0) {
    lastActivity = millis();
  }
  if (powerReleasedSinceWake && millis() >= allowSleepAt && gpio.isPressed(HalGPIO::BTN_POWER) &&
      gpio.getPowerButtonHeldTime() > 800) {
    LOG_INF("SLP", "Power hold, sleeping");
    enterDeepSleep(gpio);
    return true;
  }

  const unsigned long idleMs = millis() - lastActivity;
  const unsigned long trueMs = settings.trueSleepTimeoutMs();
  if (trueMs > 0 && idleMs >= trueMs) {
    LOG_INF("SLP", "Idle timeout, deep sleep");
    enterDeepSleep(gpio);
    return true;
  }
  const unsigned long clockMs = settings.clockModeTimeoutMs();
  if (clockMs > 0 && !screenManager.isClock() && idleMs >= clockMs) {
    LOG_INF("SLP", "Idle timeout, clock mode");
    screenManager.goToClock();
  }
  return false;
}

bool power::maybeEnterClock(HalGPIO& gpio) {
  // The wake-hold release (finger still down right after waking from deep
  // sleep) must not also open the clock.
  if (wakePowerReleasePending) {
    return false;
  }
  if (screenManager.isClock()) {
    return false;
  }
  if (gpio.wasReleased(HalGPIO::BTN_POWER) && gpio.getPowerButtonHeldTime() <= kShortPressMs) {
    if (screenManager.blocksSleep()) {
      return false;
    }
    LOG_INF("SLP", "Power short, clock mode");
    screenManager.goToClock();
    return true;
  }
  return false;
}

void power::idleDelay() {
  // The idle power-saving delay only matters on the reader and clock screens,
  // where the device typically sits for long stretches between button presses.
  // Other screens (home, settings, browser, file transfer) are short, active
  // interactions and shouldn't be throttled — the idle clock-mode timeout in
  // maybeSleep() still applies regardless.
  if (!screenManager.isReader() && !screenManager.isClock()) {
    delay(1);
    return;
  }
  if (millis() - lastActivity >= HalPowerManager::IDLE_POWER_SAVING_MS) {
    powerManager.setPowerSaving(true);
    delay(50);
  } else {
    delay(10);
  }
}
