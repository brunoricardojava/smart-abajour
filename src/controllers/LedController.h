#pragma once

#include <Arduino.h>

#include "AppState.h"

class LedController {
 public:
  explicit LedController(AppState& state);

  void begin();
  bool update();

 private:
  AppState& state_;
  uint32_t lastAdcReadMs_ = 0;
  uint16_t lastDutyCycle_ = UINT16_MAX;
  uint16_t potentiometerReferenceAdc_ = 0;
  uint8_t takeoverConfirmations_ = 0;
  bool referenceInitialized_ = false;
  bool observedPowerOn_ = false;
  ControlMode observedControlMode_ = ControlMode::Potentiometer;
  uint32_t observedWebInteractionRevision_ = 0;

  bool readPotentiometer();
  void applyOutput();
};
