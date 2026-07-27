#include "status/StatusPattern.h"

static_assert(statusIndicatorLevelAt(StatusIndicatorState::Initializing, 1234),
              "initializing must stay on");
static_assert(statusIndicatorLevelAt(StatusIndicatorState::Normal, 59),
              "normal pulse must stay on for 60 ms");
static_assert(!statusIndicatorLevelAt(StatusIndicatorState::Normal, 60),
              "normal pulse must turn off after 60 ms");
static_assert(statusIndicatorLevelAt(StatusIndicatorState::Normal, 3000),
              "normal pulse must repeat");
static_assert(statusIndicatorLevelAt(StatusIndicatorState::Portal, 275),
              "portal must emit a second pulse");
static_assert(!statusIndicatorLevelAt(StatusIndicatorState::Portal, 350),
              "portal second pulse must end");
static_assert(statusIndicatorLevelAt(StatusIndicatorState::Reconnecting, 199),
              "reconnect pulse must stay on for 200 ms");
static_assert(!statusIndicatorLevelAt(StatusIndicatorState::Reconnecting, 200),
              "reconnect pulse must turn off after 200 ms");
static_assert(statusIndicatorLevelAt(StatusIndicatorState::OtaChecking, 499),
              "OTA check must blink slowly");
static_assert(!statusIndicatorLevelAt(StatusIndicatorState::OtaChecking, 500),
              "OTA check off phase must start at 500 ms");
static_assert(statusIndicatorLevelAt(StatusIndicatorState::UpdateAvailable, 275),
              "available update must emit a second pulse");
static_assert(statusIndicatorLevelAt(StatusIndicatorState::OtaDownloading, 99),
              "download pulse must stay on for 100 ms");
static_assert(!statusIndicatorLevelAt(StatusIndicatorState::OtaDownloading, 100),
              "download pulse must turn off after 100 ms");
static_assert(statusIndicatorLevelAt(StatusIndicatorState::OtaVerifying, 899),
              "verification must remain mostly on");
static_assert(!statusIndicatorLevelAt(StatusIndicatorState::OtaVerifying, 900),
              "verification must include a short off phase");
static_assert(statusIndicatorLevelAt(StatusIndicatorState::Restarting, 1234),
              "restart must stay on");
static_assert(statusIndicatorLevelAt(StatusIndicatorState::OtaError, 450),
              "OTA error must emit three pulses");
static_assert(!statusIndicatorLevelAt(StatusIndicatorState::OtaError, 500),
              "OTA error must turn off after the third pulse");
