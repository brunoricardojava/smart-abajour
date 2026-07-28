#include "controllers/LedController.h"

#include <driver/touch_sensor.h>
#include <soc/touch_sensor_channel.h>

#include "AppConfig.h"

static_assert(AppConfig::TOUCH_PIN == TOUCH_PAD_NUM8_GPIO_NUM,
              "Configured touch GPIO must match T8");

LedController::LedController(AppState& state) : state_(state) {}

void LedController::begin() {
  analogReadResolution(AppConfig::PWM_RESOLUTION_BITS);
  analogSetPinAttenuation(AppConfig::POT_PIN, ADC_11db);

  ledcSetup(AppConfig::PWM_CHANNEL, AppConfig::PWM_FREQUENCY_HZ,
            AppConfig::PWM_RESOLUTION_BITS);
  ledcAttachPin(AppConfig::LED_PIN, AppConfig::PWM_CHANNEL);
  ledcWrite(AppConfig::PWM_CHANNEL, 0);

  lastTouchReadMs_ = millis();
  touchButtonState_ =
      TouchButtonState(false, false, lastTouchReadMs_, lastTouchReadMs_);

  esp_err_t touchStatus = touch_pad_init();
  if (touchStatus == ESP_OK) {
    touchStatus = touch_pad_set_voltage(TOUCH_HVOLT_2V7, TOUCH_LVOLT_0V5,
                                        TOUCH_HVOLT_ATTEN_1V);
  }
  if (touchStatus == ESP_OK) {
    touchStatus = touch_pad_set_cnt_mode(
        TOUCH_PAD_NUM8, TOUCH_PAD_SLOPE_7, TOUCH_PAD_TIE_OPT_LOW);
  }
  if (touchStatus == ESP_OK) {
    touchStatus = touch_pad_set_fsm_mode(TOUCH_FSM_MODE_TIMER);
  }
  if (touchStatus == ESP_OK) {
    touchStatus =
        touch_pad_config(TOUCH_PAD_NUM8, SOC_TOUCH_PAD_THRESHOLD_MAX);
  }
  if (touchStatus == ESP_OK) {
    touchStatus = touch_pad_filter_start(AppConfig::TOUCH_FILTER_PERIOD_MS);
  }

  touchDriverReady_ = touchStatus == ESP_OK;
  if (!touchDriverReady_) {
    Serial.printf("Falha ao configurar touch T8: %s\n",
                  esp_err_to_name(touchStatus));
  }
}

void LedController::calibrateTouch() {
  if (!touchDriverReady_) {
    return;
  }

  // Calibrate with the potentiometer and PWM already in their operating state.
  readPotentiometer(AppConfig::ADC_ACTIVE_SAMPLES);
  applyOutput();
  delay(AppConfig::TOUCH_FILTER_WARMUP_MS);

  const uint16_t maximumAttempts =
      AppConfig::TOUCH_CALIBRATION_SAMPLES *
      AppConfig::TOUCH_CALIBRATION_MAX_ATTEMPT_MULTIPLIER;
  for (uint16_t attempt = 0;
       attempt < maximumAttempts && !touchCalibrated_; ++attempt) {
    readTouch(millis());
    if (attempt + 1U < maximumAttempts && !touchCalibrated_) {
      delay(AppConfig::TOUCH_READ_INTERVAL_MS);
    }
  }
  lastTouchReadMs_ = millis();

  if (!touchCalibrated_) {
    Serial.println("Touch ainda sem calibracao valida; tentando no loop");
  }
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
  if (!touchDriverReady_) {
    return false;
  }

  const esp_err_t rawStatus =
      touch_pad_read_raw_data(TOUCH_PAD_NUM8, &touchRawValue_);
  const esp_err_t filteredStatus =
      touch_pad_read_filtered(TOUCH_PAD_NUM8, &touchValue_);
  if (rawStatus != ESP_OK || filteredStatus != ESP_OK || touchRawValue_ == 0 ||
      touchValue_ == 0) {
    touchButtonState_ = cancelTouchCandidate(touchButtonState_, now);
    touchRecoveryReleaseCandidate_ = false;
    if (consecutiveInvalidTouchReadings_ <
        AppConfig::TOUCH_INVALID_WARNING_COUNT) {
      ++consecutiveInvalidTouchReadings_;
      if (consecutiveInvalidTouchReadings_ ==
          AppConfig::TOUCH_INVALID_WARNING_COUNT) {
        Serial.println("Touch indisponivel; leituras zero consecutivas");
      }
    }
    return false;
  }
  consecutiveInvalidTouchReadings_ = 0;

  if (!touchCalibrated_) {
    addTouchCalibrationSample(touchValue_, now);
    return false;
  }

  const uint16_t releaseThreshold = touchThreshold(
      touchBaseline_, AppConfig::TOUCH_RELEASE_PERCENT);
  if (touchRecoveryPending_) {
    if (touchValue_ <= releaseThreshold) {
      touchRecoveryReleaseCandidate_ = false;
      return false;
    }
    if (!touchRecoveryReleaseCandidate_) {
      touchRecoveryReleaseCandidate_ = true;
      touchRecoverySinceMs_ = now;
      return false;
    }
    if (static_cast<uint32_t>(now - touchRecoverySinceMs_) <
        AppConfig::TOUCH_RELEASE_DEBOUNCE_MS) {
      return false;
    }

    Serial.println("Touch liberado; iniciando nova calibracao");
    touchRecoveryPending_ = false;
    touchRecoveryReleaseCandidate_ = false;
    resetTouchCalibration();
    touchButtonState_ = TouchButtonState(false, false, now, now);
    return false;
  }

  if (touchNeedsRecalibration(touchButtonState_, now,
                              AppConfig::TOUCH_MAXIMUM_HOLD_MS)) {
    Serial.println("Touch mantido por muito tempo; aguardando liberacao");
    touchRecoveryPending_ = true;
    touchRecoveryReleaseCandidate_ = false;
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

bool LedController::addTouchCalibrationSample(uint16_t value, uint32_t now) {
  touchCalibrationValues_[touchCalibrationSamples_++] = value;
  if (touchCalibrationSamples_ < AppConfig::TOUCH_CALIBRATION_SAMPLES) {
    return false;
  }

  for (uint8_t index = 1; index < AppConfig::TOUCH_CALIBRATION_SAMPLES;
       ++index) {
    const uint16_t sortedValue = touchCalibrationValues_[index];
    uint8_t position = index;
    while (position > 0 &&
           touchCalibrationValues_[position - 1U] > sortedValue) {
      touchCalibrationValues_[position] =
          touchCalibrationValues_[position - 1U];
      --position;
    }
    touchCalibrationValues_[position] = sortedValue;
  }

  constexpr uint8_t first = AppConfig::TOUCH_CALIBRATION_TRIM_SAMPLES;
  constexpr uint8_t last = AppConfig::TOUCH_CALIBRATION_SAMPLES -
                           AppConfig::TOUCH_CALIBRATION_TRIM_SAMPLES;
  uint32_t sum = 0;
  for (uint8_t index = first; index < last; ++index) {
    sum += touchCalibrationValues_[index];
  }
  const uint8_t retainedSamples = last - first;
  const uint16_t baseline = static_cast<uint16_t>(sum / retainedSamples);
  const uint16_t spread =
      touchCalibrationValues_[last - 1U] - touchCalibrationValues_[first];
  touchCalibrationSamples_ = 0;
  if (!touchCalibrationValid(
          baseline, spread,
          AppConfig::TOUCH_CALIBRATION_MAX_SPREAD_PERCENT)) {
    Serial.printf("Calibracao touch instavel: base %u, dispersao %u\n",
                  baseline, spread);
    return false;
  }

  touchBaseline_ = baseline;
  touchBaselineScaled_ =
      static_cast<uint32_t>(touchBaseline_) * AppConfig::TOUCH_BASELINE_SCALE;
  touchButtonState_ = TouchButtonState(false, false, now, now);
  touchCalibrated_ = true;
  Serial.printf("Touch GPIO%u/T8 calibrado: base %u, dispersao %u\n",
                AppConfig::TOUCH_PIN, touchBaseline_, spread);
  return true;
}

void LedController::resetTouchCalibration() {
  touchCalibrated_ = false;
  touchCalibrationSamples_ = 0;
  touchBaseline_ = 0;
  touchBaselineScaled_ = 0;
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
