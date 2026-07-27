#pragma once

#include <Arduino.h>
#include <WiFiManager.h>

class NetworkManager {
 public:
  void begin();
  void update();

  bool isConnected() const;
  bool isPortalActive() const;
  bool isResetPending() const;
  String ipAddress() const;
  int32_t signalStrength() const;
  const String& setupAccessPointName() const;
  void scheduleCredentialsReset();

 private:
  WiFiManager wifiManager_;
  String setupAccessPointName_;
  bool portalActive_ = false;
  bool wasConnected_ = false;
  bool mdnsStarted_ = false;
  bool resetPending_ = false;
  uint32_t resetAtMs_ = 0;

  void handleConnectionChange(bool connected);
};
