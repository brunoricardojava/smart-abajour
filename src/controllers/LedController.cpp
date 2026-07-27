#include "controllers/LedController.h"

#include "AppConfig.h"

LedController::LedController(AppState& state) : state_(state) {}

void LedController::begin() {
  analogReadResolution(AppConfig::PWM_RESOLUTION_BITS);
  analogSetPinAttenuation(AppConfig::POT_PIN, ADC_11db);

  ledcSetup(AppConfig::PWM_CHANNEL, AppConfig::PWM_FREQUENCY_HZ,
            AppConfig::PWM_RESOLUTION_BITS);
  ledcAttachPin(AppConfig::LED_PIN, AppConfig::PWM_CHANNEL);
  ledcWrite(AppConfig::PWM_CHANNEL, 0);
}

bool LedController::update() {
  if (state_.controlMode != observedControlMode_ ||
      state_.powerOn != observedPowerOn_ ||
      state_.webInteractionRevision != observedWebInteractionRevision_) {
    observedControlMode_ = state_.controlMode;
    observedPowerOn_ = state_.powerOn;
    observedWebInteractionRevision_ = state_.webInteractionRevision;
    potentiometerReferenceAdc_ = state_.potentiometerAdc;
    takeoverConfirmations_ = 0;
  }

  bool settingsChanged = false;
  const uint32_t now = millis();
  const bool waitingForPhysicalInteraction =
      state_.controlMode == ControlMode::Web || !state_.powerOn;
  const uint32_t adcInterval =
      waitingForPhysicalInteraction ? AppConfig::ADC_IDLE_INTERVAL_MS
                                    : AppConfig::ADC_ACTIVE_INTERVAL_MS;
  const uint8_t adcSamples =
      waitingForPhysicalInteraction ? AppConfig::ADC_IDLE_SAMPLES
                                    : AppConfig::ADC_ACTIVE_SAMPLES;
  if (now - lastAdcReadMs_ >= adcInterval) {
    lastAdcReadMs_ = now;
    settingsChanged = readPotentiometer(adcSamples);
  }

  applyOutput();
  return settingsChanged;
}

bool LedController::readPotentiometer(uint8_t samples) {
  uint32_t sum = 0;
  for (uint8_t sample = 0; sample < samples; ++sample) {
    sum += analogRead(AppConfig::POT_PIN);
  }

  state_.potentiometerAdc = sum / samples;
  state_.potentiometerBrightness = static_cast<uint8_t>(
      (static_cast<uint32_t>(state_.potentiometerAdc) * 100U +
       AppConfig::PWM_MAX / 2U) /
      AppConfig::PWM_MAX);

  if (!referenceInitialized_) {
    potentiometerReferenceAdc_ = state_.potentiometerAdc;
    referenceInitialized_ = true;
    observedPowerOn_ = state_.powerOn;
    observedControlMode_ = state_.controlMode;
    observedWebInteractionRevision_ = state_.webInteractionRevision;
    return false;
  }

  const bool waitingForPhysicalInteraction =
      state_.controlMode == ControlMode::Web || !state_.powerOn;
  if (!waitingForPhysicalInteraction) {
    potentiometerReferenceAdc_ = state_.potentiometerAdc;
    takeoverConfirmations_ = 0;
    return false;
  }

  const uint16_t movement =
      state_.potentiometerAdc > potentiometerReferenceAdc_
          ? state_.potentiometerAdc - potentiometerReferenceAdc_
          : potentiometerReferenceAdc_ - state_.potentiometerAdc;
  if (movement < AppConfig::POT_TAKEOVER_THRESHOLD) {
    takeoverConfirmations_ = 0;
    return false;
  }

  if (++takeoverConfirmations_ < AppConfig::POT_TAKEOVER_CONFIRMATIONS) {
    return false;
  }

  takeoverConfirmations_ = 0;
  potentiometerReferenceAdc_ = state_.potentiometerAdc;
  state_.controlMode = ControlMode::Potentiometer;
  state_.powerOn = true;
  observedControlMode_ = state_.controlMode;
  observedPowerOn_ = state_.powerOn;
  Serial.println("Potenciometro assumiu o controle");
  return true;
}

void LedController::applyOutput() {
  const uint8_t selectedBrightness =
      state_.controlMode == ControlMode::Potentiometer
          ? state_.potentiometerBrightness
          : state_.manualBrightness;
  state_.outputBrightness = state_.powerOn ? selectedBrightness : 0;

  uint16_t dutyCycle = 0;
  if (state_.powerOn) {
    dutyCycle = state_.controlMode == ControlMode::Potentiometer
                    ? state_.potentiometerAdc
                    : static_cast<uint16_t>(
                          (static_cast<uint32_t>(state_.manualBrightness) *
                               AppConfig::PWM_MAX +
                           50U) /
                          100U);
  }

  if (dutyCycle != lastDutyCycle_) {
    lastDutyCycle_ = dutyCycle;
    ledcWrite(AppConfig::PWM_CHANNEL, dutyCycle);
  }
}
