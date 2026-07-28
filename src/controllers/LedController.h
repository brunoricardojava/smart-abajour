#pragma once

#include <Arduino.h>

#include "AppState.h"
#include "input/TouchButtonState.h"

class LedController {
 public:
  explicit LedController(AppState& state);

  void begin();
  void calibrateTouch();
  bool update();
  uint16_t touchValue() const { return touchValue_; }
  uint16_t touchBaseline() const { return touchBaseline_; }

 private:
  AppState& state_;
  uint32_t lastAdcReadMs_ = 0;
  uint32_t lastTouchReadMs_ = 0;
  uint16_t lastDutyCycle_ = UINT16_MAX;
  uint16_t potentiometerReferenceAdc_ = 0;
  uint8_t takeoverConfirmations_ = 0;
  bool referenceInitialized_ = false;
  bool observedPowerOn_ = false;
  ControlMode observedControlMode_ = ControlMode::Potentiometer;
  uint32_t observedWebInteractionRevision_ = 0;
  TouchButtonState touchButtonState_;
  uint32_t touchCalibrationSum_ = 0;
  uint32_t touchBaselineScaled_ = 0;
  uint16_t touchValue_ = 0;
  uint16_t touchBaseline_ = 0;
  uint8_t touchCalibrationSamples_ = 0;
  uint8_t consecutiveInvalidTouchReadings_ = 0;
  bool touchCalibrated_ = false;

  bool readPotentiometer(uint8_t samples);
  bool readTouch(uint32_t now);
  void applyOutput();
};
