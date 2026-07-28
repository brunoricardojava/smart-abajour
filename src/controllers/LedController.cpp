#include "controllers/LedController.h"

#include <driver/touch_sensor.h>

#include "AppConfig.h"

LedController::LedController(AppState& state) : state_(state) {}

void LedController::begin() {
  analogReadResolution(AppConfig::PWM_RESOLUTION_BITS);
  analogSetPinAttenuation(AppConfig::POT_PIN, ADC_11db);

  ledcSetup(AppConfig::PWM_CHANNEL, AppConfig::PWM_FREQUENCY_HZ,
            AppConfig::PWM_RESOLUTION_BITS);
  ledcAttachPin(AppConfig::LED_PIN, AppConfig::PWM_CHANNEL);
  ledcWrite(AppConfig::PWM_CHANNEL, 0);

  lastTouchReadMs_ = millis();
  touchButtonState_ = TouchButtonState(false, false, lastTouchReadMs_);
}

void LedController::calibrateTouch() {
  for (uint8_t sample = 0; sample < AppConfig::TOUCH_CALIBRATION_SAMPLES;
       ++sample) {
    readTouch(millis());
    if (sample + 1U < AppConfig::TOUCH_CALIBRATION_SAMPLES) {
      delay(AppConfig::TOUCH_READ_INTERVAL_MS);
    }
  }
  lastTouchReadMs_ = millis();
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
  if (now - lastTouchReadMs_ >= AppConfig::TOUCH_READ_INTERVAL_MS) {
    lastTouchReadMs_ = now;
    settingsChanged |= readTouch(now);
  }
  if (now - lastAdcReadMs_ >= adcInterval) {
    lastAdcReadMs_ = now;
    settingsChanged |= readPotentiometer(adcSamples);
  }

  applyOutput();
  return settingsChanged;
}

bool LedController::readPotentiometer(uint8_t samples) {
  uint32_t sum = 0;
  for (uint8_t sample = 0; sample < samples; ++sample) {
    sum += analogRead(AppConfig::POT_PIN);
  }

  // ADC on GPIO32 retasks the shared RTC peripheral. Restore T8 now so it has
  // a full sampling interval to settle before the next capacitive reading.
  touch_pad_config(TOUCH_PAD_NUM8, SOC_TOUCH_PAD_THRESHOLD_MAX);

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

bool LedController::readTouch(uint32_t now) {
  touchValue_ = touchRead(AppConfig::TOUCH_PIN);
  if (touchValue_ == 0) {
    static uint32_t lastInvalidReadingLogMs = 0;
    if (now - lastInvalidReadingLogMs >= AppConfig::SERIAL_INTERVAL_MS) {
      lastInvalidReadingLogMs = now;
      Serial.println("Leitura touch invalida (0); evento ignorado");
    }
    return false;
  }

  if (!touchCalibrated_) {
    touchCalibrationSum_ += touchValue_;
    if (++touchCalibrationSamples_ < AppConfig::TOUCH_CALIBRATION_SAMPLES) {
      return false;
    }

    touchBaseline_ = static_cast<uint16_t>(
        touchCalibrationSum_ / AppConfig::TOUCH_CALIBRATION_SAMPLES);
    touchCalibrationSum_ = 0;
    touchCalibrationSamples_ = 0;
    if (touchBaseline_ == 0) {
      Serial.println("Falha ao calibrar touch; nova tentativa agendada");
      return false;
    }

    touchBaselineScaled_ =
        static_cast<uint32_t>(touchBaseline_) * AppConfig::TOUCH_BASELINE_SCALE;
    touchButtonState_ = TouchButtonState(false, false, now);
    touchCalibrated_ = true;
    Serial.printf("Touch GPIO%u calibrado: base %u\n", AppConfig::TOUCH_PIN,
                  touchBaseline_);
    return false;
  }

  if (touchNeedsRecalibration(touchButtonState_, now,
                              AppConfig::TOUCH_MAXIMUM_HOLD_MS)) {
    Serial.println("Touch mantido por muito tempo; recalibrando entrada");
    touchCalibrated_ = false;
    touchCalibrationSum_ = touchValue_;
    touchCalibrationSamples_ = 1;
    touchBaseline_ = 0;
    touchBaselineScaled_ = 0;
    touchButtonState_ = TouchButtonState(false, false, now);
    return false;
  }

  const uint16_t threshold = touchThreshold(
      touchBaseline_, touchButtonState_.stableTouched
                          ? AppConfig::TOUCH_RELEASE_PERCENT
                          : AppConfig::TOUCH_PRESS_PERCENT);
  const bool rawTouched = touchValue_ <= threshold;
  const TouchButtonResult result = updateTouchButton(
      touchButtonState_, rawTouched, now, AppConfig::TOUCH_PRESS_DEBOUNCE_MS,
      AppConfig::TOUCH_RELEASE_DEBOUNCE_MS);
  touchButtonState_ = result.state;

  if (!rawTouched && !touchButtonState_.stableTouched) {
    const int32_t scaledValue =
        static_cast<int32_t>(touchValue_) * AppConfig::TOUCH_BASELINE_SCALE;
    const int32_t scaledBaseline =
        static_cast<int32_t>(touchBaselineScaled_);
    touchBaselineScaled_ = static_cast<uint32_t>(
        scaledBaseline + (scaledValue - scaledBaseline) /
                             AppConfig::TOUCH_BASELINE_FILTER_DIVISOR);
    touchBaseline_ = static_cast<uint16_t>(
        (touchBaselineScaled_ + AppConfig::TOUCH_BASELINE_SCALE / 2U) /
        AppConfig::TOUCH_BASELINE_SCALE);
  }

  if (!result.pressed) {
    return false;
  }

  state_.powerOn = !state_.powerOn;
  observedPowerOn_ = state_.powerOn;
  potentiometerReferenceAdc_ = state_.potentiometerAdc;
  takeoverConfirmations_ = 0;
  Serial.printf("Touch fisico: LED %s (leitura %u, base %u)\n",
                state_.powerOn ? "ligado" : "desligado", touchValue_,
                touchBaseline_);
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
