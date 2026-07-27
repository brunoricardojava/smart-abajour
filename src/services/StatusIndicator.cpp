#include "services/StatusIndicator.h"

#include "AppConfig.h"

void StatusIndicator::begin() {
  digitalWrite(AppConfig::STATUS_LED_PIN,
               AppConfig::STATUS_LED_ACTIVE_HIGH ? LOW : HIGH);
  pinMode(AppConfig::STATUS_LED_PIN, OUTPUT);
  stateStartedAtMs_ = millis();
  setOutput(true);
  Serial.println("Indicador azul: inicializando");
}

void StatusIndicator::update(uint32_t now, bool networkConnected,
                             bool portalActive, bool resetPending,
                             OtaStatus otaStatus) {
  const StatusIndicatorState nextState =
      resolveState(networkConnected, portalActive, resetPending, otaStatus);
  if (nextState != state_) {
    state_ = nextState;
    stateStartedAtMs_ = now;
    Serial.printf("Indicador azul: %s\n", stateName(state_));
  }

  setOutput(statusIndicatorLevelAt(state_, now - stateStartedAtMs_));
}

StatusIndicatorState StatusIndicator::resolveState(bool networkConnected,
                                                   bool portalActive,
                                                   bool resetPending,
                                                   OtaStatus otaStatus) {
  if (resetPending || otaStatus == OtaStatus::Rebooting) {
    return StatusIndicatorState::Restarting;
  }
  if (otaStatus == OtaStatus::Downloading) {
    return StatusIndicatorState::OtaDownloading;
  }
  if (otaStatus == OtaStatus::Verifying) {
    return StatusIndicatorState::OtaVerifying;
  }
  if (otaStatus == OtaStatus::Checking) {
    return StatusIndicatorState::OtaChecking;
  }
  if (portalActive) {
    return StatusIndicatorState::Portal;
  }
  if (!networkConnected) {
    return StatusIndicatorState::Reconnecting;
  }
  if (otaStatus == OtaStatus::Error) {
    return StatusIndicatorState::OtaError;
  }
  if (otaStatus == OtaStatus::Available) {
    return StatusIndicatorState::UpdateAvailable;
  }
  return StatusIndicatorState::Normal;
}

const char* StatusIndicator::stateName(StatusIndicatorState state) {
  switch (state) {
    case StatusIndicatorState::Initializing:
      return "inicializando";
    case StatusIndicatorState::Normal:
      return "operacao normal";
    case StatusIndicatorState::Portal:
      return "portal de configuracao";
    case StatusIndicatorState::Reconnecting:
      return "reconectando Wi-Fi";
    case StatusIndicatorState::OtaChecking:
      return "consultando atualizacao";
    case StatusIndicatorState::UpdateAvailable:
      return "atualizacao disponivel";
    case StatusIndicatorState::OtaDownloading:
      return "baixando firmware";
    case StatusIndicatorState::OtaVerifying:
      return "verificando firmware";
    case StatusIndicatorState::Restarting:
      return "reiniciando";
    case StatusIndicatorState::OtaError:
      return "erro OTA";
  }
  return "desconhecido";
}

void StatusIndicator::setOutput(bool on) {
  if (outputInitialized_ && outputOn_ == on) {
    return;
  }
  outputInitialized_ = true;
  outputOn_ = on;
  digitalWrite(AppConfig::STATUS_LED_PIN,
               on == AppConfig::STATUS_LED_ACTIVE_HIGH ? HIGH : LOW);
}
