#include "input/TouchButtonState.h"

constexpr TouchButtonState initialState(false, false, 0);
constexpr TouchButtonResult isolatedLow =
    updateTouchButton(initialState, true, 100, 20, 300);
static_assert(!isolatedLow.pressed && isolatedLow.state.candidateTouched,
              "one low sample must only start press qualification");

constexpr TouchButtonState cancelled =
    cancelTouchCandidate(isolatedLow.state, 110);
static_assert(!cancelled.stableTouched && !cancelled.candidateTouched,
              "an invalid sample must cancel a pending press");

constexpr TouchButtonResult pressCandidate =
    updateTouchButton(cancelled, true, 120, 20, 300);
constexpr TouchButtonResult pressed =
    updateTouchButton(pressCandidate.state, true, 140, 20, 300);
static_assert(pressed.pressed && pressed.state.stableTouched &&
                  pressed.state.stableSinceMs == 140,
              "qualified touch must emit one press event");

constexpr TouchButtonResult held =
    updateTouchButton(pressed.state, true, 1000, 20, 300);
static_assert(!held.pressed && held.state.stableTouched,
              "held touch must not repeat the press event");

constexpr TouchButtonResult releaseCandidate =
    updateTouchButton(held.state, false, 1100, 20, 300);
constexpr TouchButtonResult releaseTooSoon =
    updateTouchButton(releaseCandidate.state, false, 1399, 20, 300);
static_assert(releaseTooSoon.state.stableTouched,
              "release must remain pending during debounce");
constexpr TouchButtonResult released =
    updateTouchButton(releaseTooSoon.state, false, 1400, 20, 300);
static_assert(!released.pressed && !released.state.stableTouched,
              "stable release must rearm without a press event");

constexpr TouchButtonResult secondPressCandidate =
    updateTouchButton(released.state, true, 1500, 20, 300);
constexpr TouchButtonResult secondPress =
    updateTouchButton(secondPressCandidate.state, true, 1520, 20, 300);
static_assert(secondPress.pressed,
              "a new touch after release must emit another event");

static_assert(!touchNeedsRecalibration(pressed.state, 15139, 15000),
              "held touch must remain active before the recovery timeout");
static_assert(touchNeedsRecalibration(pressed.state, 15140, 15000),
              "held touch must trigger recovery at the timeout");
static_assert(touchThreshold(100, 70) == 70,
              "touch threshold must be relative to the baseline");
static_assert(touchCalibrationValid(100, 15, 15),
              "calibration must accept its maximum configured spread");
static_assert(!touchCalibrationValid(100, 16, 15),
              "calibration must reject excessive spread");
static_assert(!touchCalibrationValid(0, 0, 15) &&
                  !touchCalibrationValid(UINT16_MAX, 0, 15),
              "calibration must reject counter rails");
