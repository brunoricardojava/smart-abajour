#pragma once

#include <Arduino.h>

#include "services/OtaUpdateService.h"
#include "status/StatusPattern.h"

class StatusIndicator {
 public:
  void begin();
  void update(uint32_t now, bool networkConnected, bool portalActive,
              bool resetPending, OtaStatus otaStatus);

 private:
  StatusIndicatorState state_ = StatusIndicatorState::Initializing;
  uint32_t stateStartedAtMs_ = 0;
  bool outputInitialized_ = false;
  bool outputOn_ = false;

  static StatusIndicatorState resolveState(bool networkConnected,
                                           bool portalActive,
                                           bool resetPending,
                                           OtaStatus otaStatus);
  static const char* stateName(StatusIndicatorState state);
  void setOutput(bool on);
};
