#pragma once

#include <stdint.h>

enum class StatusIndicatorState : uint8_t {
  Initializing,
  Normal,
  Portal,
  Reconnecting,
  OtaChecking,
  UpdateAvailable,
  OtaDownloading,
  OtaVerifying,
  Restarting,
  OtaError,
};

constexpr bool statusIndicatorInWindow(uint32_t elapsedMs, uint32_t cycleMs,
                                       uint32_t startMs, uint32_t endMs) {
  return elapsedMs % cycleMs >= startMs && elapsedMs % cycleMs < endMs;
}

constexpr bool statusIndicatorLevelAt(StatusIndicatorState state,
                                      uint32_t elapsedMs) {
  return state == StatusIndicatorState::Initializing ||
                 state == StatusIndicatorState::Restarting
             ? true
         : state == StatusIndicatorState::Normal
             ? statusIndicatorInWindow(elapsedMs, 3000, 0, 60)
         : state == StatusIndicatorState::Portal
             ? statusIndicatorInWindow(elapsedMs, 2000, 0, 100) ||
                   statusIndicatorInWindow(elapsedMs, 2000, 250, 350)
         : state == StatusIndicatorState::Reconnecting
             ? statusIndicatorInWindow(elapsedMs, 1000, 0, 200)
         : state == StatusIndicatorState::OtaChecking
             ? statusIndicatorInWindow(elapsedMs, 1000, 0, 500)
         : state == StatusIndicatorState::UpdateAvailable
             ? statusIndicatorInWindow(elapsedMs, 5000, 0, 100) ||
                   statusIndicatorInWindow(elapsedMs, 5000, 250, 350)
         : state == StatusIndicatorState::OtaDownloading
             ? statusIndicatorInWindow(elapsedMs, 200, 0, 100)
         : state == StatusIndicatorState::OtaVerifying
             ? statusIndicatorInWindow(elapsedMs, 1000, 0, 900)
         : state == StatusIndicatorState::OtaError
             ? statusIndicatorInWindow(elapsedMs, 4000, 0, 100) ||
                   statusIndicatorInWindow(elapsedMs, 4000, 200, 300) ||
                   statusIndicatorInWindow(elapsedMs, 4000, 400, 500)
             : false;
}
