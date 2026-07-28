#include "input/TouchButtonState.h"

constexpr TouchButtonState initialState(false, false, 0);
constexpr TouchButtonResult pressCandidate =
    updateTouchButton(initialState, true, 100, 60, 100);
static_assert(!pressCandidate.pressed &&
                  pressCandidate.state.candidateTouched,
              "first touch sample must only start debounce");

constexpr TouchButtonResult pressTooSoon =
    updateTouchButton(pressCandidate.state, true, 159, 60, 100);
static_assert(!pressTooSoon.pressed && !pressTooSoon.state.stableTouched,
              "touch must remain pending during debounce");

constexpr TouchButtonResult pressed =
    updateTouchButton(pressTooSoon.state, true, 160, 60, 100);
static_assert(pressed.pressed && pressed.state.stableTouched,
              "stable touch must emit one press event");

constexpr TouchButtonResult held =
    updateTouchButton(pressed.state, true, 1000, 60, 100);
static_assert(!held.pressed && held.state.stableTouched,
              "held touch must not repeat the press event");

constexpr TouchButtonResult releaseCandidate =
    updateTouchButton(held.state, false, 1100, 60, 100);
constexpr TouchButtonResult released =
    updateTouchButton(releaseCandidate.state, false, 1200, 60, 100);
static_assert(!released.pressed && !released.state.stableTouched,
              "stable release must rearm without a press event");

constexpr TouchButtonResult secondPressCandidate =
    updateTouchButton(released.state, true, 1300, 60, 100);
constexpr TouchButtonResult secondPress =
    updateTouchButton(secondPressCandidate.state, true, 1360, 60, 100);
static_assert(secondPress.pressed,
              "a new touch after release must emit another event");

static_assert(!touchNeedsRecalibration(pressed.state, 15099, 15000),
              "held touch must remain active before the recovery timeout");
static_assert(touchNeedsRecalibration(pressed.state, 15100, 15000),
              "held touch must trigger recovery at the timeout");
static_assert(touchThreshold(100, 70) == 70,
              "touch threshold must be relative to the baseline");
