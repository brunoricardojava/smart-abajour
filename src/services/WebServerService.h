#pragma once

#include <Arduino.h>
#include <WebServer.h>

#include "AppState.h"

class NetworkManager;
class OtaUpdateService;
class SettingsStore;

class WebServerService {
 public:
  WebServerService(AppState& state, NetworkManager& network,
                   SettingsStore& settings, OtaUpdateService& ota);

  void begin();
  void update();
  bool isStarted() const;

 private:
  AppState& state_;
  NetworkManager& network_;
  SettingsStore& settings_;
  OtaUpdateService& ota_;
  WebServer server_{80};
  bool started_ = false;
  char csrfToken_[17] = {};

  void handleState();
  void handleControl();
  void handleWifiReset();
  void handleFactoryReset();
  void handleOtaCheck();
  void handleOtaInstall();
  bool authorizeMutation();
  void sendState();
  void sendError(uint16_t statusCode, const __FlashStringHelper* message);
};
