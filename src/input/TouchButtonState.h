#pragma once

#include <stdint.h>

struct TouchButtonState {
  bool stableTouched;
  bool candidateTouched;
  uint32_t candidateSinceMs;
  uint32_t stableSinceMs;

  constexpr TouchButtonState(bool stable = false, bool candidate = false,
                              uint32_t candidateSince = 0,
                              uint32_t stableSince = 0)
      : stableTouched(stable),
        candidateTouched(candidate),
        candidateSinceMs(candidateSince),
        stableSinceMs(stableSince) {}
};

struct TouchButtonResult {
  TouchButtonState state;
  bool pressed;

  constexpr TouchButtonResult(TouchButtonState nextState, bool pressedEvent)
      : state(nextState), pressed(pressedEvent) {}
};

constexpr uint16_t touchThreshold(uint16_t baseline, uint8_t percent) {
  return static_cast<uint16_t>((static_cast<uint32_t>(baseline) * percent) /
                               100U);
}

constexpr bool touchCalibrationValid(uint16_t baseline, uint16_t spread,
                                     uint8_t maximumSpreadPercent) {
  return baseline > 0 && baseline < UINT16_MAX &&
         spread <= static_cast<uint32_t>(baseline) * maximumSpreadPercent /
                       100U;
}

constexpr bool touchNeedsRecalibration(TouchButtonState state, uint32_t nowMs,
                                       uint32_t maximumHoldMs) {
  return state.stableTouched &&
         static_cast<uint32_t>(nowMs - state.stableSinceMs) >= maximumHoldMs;
}

constexpr TouchButtonState cancelTouchCandidate(TouchButtonState state,
                                                 uint32_t nowMs) {
  return TouchButtonState(state.stableTouched, state.stableTouched, nowMs,
                          state.stableSinceMs);
}

constexpr TouchButtonResult updateTouchButton(
    TouchButtonState state, bool rawTouched, uint32_t nowMs,
    uint32_t pressDebounceMs, uint32_t releaseDebounceMs) {
  return rawTouched != state.candidateTouched
             ? rawTouched != state.stableTouched &&
                       (rawTouched ? pressDebounceMs : releaseDebounceMs) == 0
                   ? TouchButtonResult(
                         TouchButtonState(rawTouched, rawTouched, nowMs, nowMs),
                         rawTouched)
                   : TouchButtonResult(
                         TouchButtonState(state.stableTouched, rawTouched, nowMs,
                                          state.stableSinceMs),
                         false)
         : rawTouched == state.stableTouched
             ? TouchButtonResult(state, false)
         : static_cast<uint32_t>(nowMs - state.candidateSinceMs) <
                   (rawTouched ? pressDebounceMs : releaseDebounceMs)
             ? TouchButtonResult(state, false)
             : TouchButtonResult(
                   TouchButtonState(rawTouched, rawTouched,
                                    state.candidateSinceMs, nowMs),
                   rawTouched);
}
