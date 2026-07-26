#pragma once

#include <Arduino.h>

enum class ControlMode : uint8_t {
  Potentiometer,
  Web,
};

struct AppState {
  bool powerOn = false;
  ControlMode controlMode = ControlMode::Potentiometer;
  uint8_t manualBrightness = 50;
  uint8_t potentiometerBrightness = 0;
  uint8_t outputBrightness = 0;
  uint16_t potentiometerAdc = 0;
  uint32_t webInteractionRevision = 0;
};
