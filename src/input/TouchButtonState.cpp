#include "input/TouchButtonState.h"

constexpr TouchButtonState initialState(false, false, 0);
constexpr TouchButtonResult pressed =
    updateTouchButton(initialState, true, 100, 0, 300);
static_assert(pressed.pressed && pressed.state.stableTouched,
              "zero debounce must emit on the first touch sample");

constexpr TouchButtonResult held =
    updateTouchButton(pressed.state, true, 1000, 0, 300);
static_assert(!held.pressed && held.state.stableTouched,
              "held touch must not repeat the press event");

constexpr TouchButtonResult releaseCandidate =
    updateTouchButton(held.state, false, 1100, 0, 300);
constexpr TouchButtonResult releaseTooSoon =
    updateTouchButton(releaseCandidate.state, false, 1399, 0, 300);
static_assert(releaseTooSoon.state.stableTouched,
              "release must remain pending during debounce");
constexpr TouchButtonResult released =
    updateTouchButton(releaseTooSoon.state, false, 1400, 0, 300);
static_assert(!released.pressed && !released.state.stableTouched,
              "stable release must rearm without a press event");

constexpr TouchButtonResult secondPress =
    updateTouchButton(released.state, true, 1500, 0, 300);
static_assert(secondPress.pressed,
              "a new touch after release must emit another event");

constexpr TouchButtonResult debouncedCandidate =
    updateTouchButton(initialState, true, 100, 20, 300);
constexpr TouchButtonResult debouncedTooSoon =
    updateTouchButton(debouncedCandidate.state, true, 119, 20, 300);
constexpr TouchButtonResult debouncedPress =
    updateTouchButton(debouncedTooSoon.state, true, 120, 20, 300);
static_assert(!debouncedCandidate.pressed && !debouncedTooSoon.pressed &&
                  debouncedPress.pressed,
              "nonzero debounce must still require stable input");

static_assert(!touchNeedsRecalibration(pressed.state, 15099, 15000),
              "held touch must remain active before the recovery timeout");
static_assert(touchNeedsRecalibration(pressed.state, 15100, 15000),
              "held touch must trigger recovery at the timeout");
static_assert(touchThreshold(100, 70) == 70,
              "touch threshold must be relative to the baseline");
