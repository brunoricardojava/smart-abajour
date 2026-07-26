#include "services/SettingsStore.h"

#include "AppConfig.h"

namespace {
constexpr char NAMESPACE[] = "led-control";
constexpr char KEY_VALID[] = "valid";
constexpr char KEY_POWER[] = "power";
constexpr char KEY_MODE[] = "mode";
constexpr char KEY_BRIGHTNESS[] = "brightness";
}  // namespace

bool SettingsStore::begin(AppState& state) {
  ready_ = preferences_.begin(NAMESPACE, false);
  if (!ready_) {
    Serial.println("Falha ao abrir o armazenamento de preferencias");
    return false;
  }

  if (preferences_.getBool(KEY_VALID, false)) {
    state.powerOn = preferences_.getBool(KEY_POWER, false);
    state.controlMode = preferences_.getBool(KEY_MODE, true)
                            ? ControlMode::Potentiometer
                            : ControlMode::Web;
    state.manualBrightness = constrain(
        preferences_.getUChar(KEY_BRIGHTNESS, 50), 0, 100);
  }

  return true;
}

void SettingsStore::scheduleSave() {
  if (!ready_) {
    return;
  }

  savePending_ = true;
  saveRequestedAtMs_ = millis();
}

void SettingsStore::update(const AppState& state) {
  if (!savePending_ ||
      millis() - saveRequestedAtMs_ < AppConfig::SETTINGS_SAVE_DELAY_MS) {
    return;
  }

  savePending_ = false;
  save(state);
}

bool SettingsStore::restoreFactoryDefaults(AppState& state) {
  if (!ready_) {
    return false;
  }

  savePending_ = false;
  if (!preferences_.clear()) {
    Serial.println("Falha ao apagar as preferencias do LED");
    return false;
  }

  state = AppState{};
  Serial.println("Preferencias do LED restauradas");
  return true;
}

void SettingsStore::save(const AppState& state) {
  preferences_.putBool(KEY_POWER, state.powerOn);
  preferences_.putBool(KEY_MODE,
                       state.controlMode == ControlMode::Potentiometer);
  preferences_.putUChar(KEY_BRIGHTNESS, state.manualBrightness);
  preferences_.putBool(KEY_VALID, true);
}
