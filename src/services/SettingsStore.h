#pragma once

#include <Arduino.h>
#include <Preferences.h>

#include "AppState.h"

class SettingsStore {
 public:
  bool begin(AppState& state);
  void scheduleSave();
  void update(const AppState& state);
  bool restoreFactoryDefaults(AppState& state);

 private:
  Preferences preferences_;
  bool ready_ = false;
  bool savePending_ = false;
  uint32_t saveRequestedAtMs_ = 0;

  void save(const AppState& state);
};
