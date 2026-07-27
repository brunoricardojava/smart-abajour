#include "services/NetworkManager.h"

#include <ESPmDNS.h>
#include <WiFi.h>

#include "AppConfig.h"

void NetworkManager::begin() {
  char accessPointName[32];
  const uint16_t chipSuffix =
      static_cast<uint16_t>(ESP.getEfuseMac() & 0xFFFFU);
  snprintf(accessPointName, sizeof(accessPointName), "%s-%04X",
           AppConfig::SETUP_AP_PREFIX, chipSuffix);
  setupAccessPointName_ = accessPointName;

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(WIFI_PS_MIN_MODEM);
  WiFi.setHostname(AppConfig::HOSTNAME);
  WiFi.setAutoReconnect(true);

  wifiManager_.setConfigPortalBlocking(false);
  wifiManager_.setConnectTimeout(AppConfig::WIFI_CONNECT_TIMEOUT_SECONDS);
  wifiManager_.setConfigPortalTimeout(0);
  wifiManager_.setCaptivePortalEnable(true);
  wifiManager_.setEnableConfigPortal(true);
  wifiManager_.setDisableConfigPortal(false);
  wifiManager_.setWiFiAutoReconnect(true);
  // Samsung/Android may ignore captive DNS replies that resolve to a private IP.
  wifiManager_.setAPStaticIPConfig(IPAddress(8, 8, 8, 8),
                                    IPAddress(8, 8, 8, 8),
                                    IPAddress(255, 255, 255, 0));
  wifiManager_.setTitle("Configurar Wi-Fi do LED");
  wifiManager_.setDarkMode(true);
  wifiManager_.setShowPassword(false);
  wifiManager_.setShowInfoErase(false);
  wifiManager_.setShowInfoUpdate(false);
  wifiManager_.setWebServerCallback([this]() {
    // WiFiManager registers an unverified binary upload route after this
    // callback. Registering first shadows it while preserving the portal.
    wifiManager_.server->on("/update", HTTP_ANY, [this]() {
      wifiManager_.server->send(404, "text/plain", "Not found");
    });
    wifiManager_.server->on(
        "/u", HTTP_POST,
        [this]() {
          wifiManager_.server->send(404, "text/plain", "Not found");
        },
        []() {});
  });

  const char* portalMenu[] = {"wifi"};
  wifiManager_.setMenu(portalMenu, 1);
  wifiManager_.setCustomHeadElement(
      "<meta name='theme-color' content='#0b1712'>"
      "<style>body{background:#0b1712;color:#effff3}"
      ".wrap:before{content:'Escolha sua rede Wi-Fi para conectar o LED';"
      "display:block;margin:0 0 18px;color:#8fe7a9;font-size:14px;"
      "line-height:1.5}button{border-radius:12px!important}</style>");

  Serial.println("Tentando conectar ao Wi-Fi salvo...");
  const bool connected = wifiManager_.autoConnect(
      setupAccessPointName_.c_str(), AppConfig::SETUP_AP_PASSWORD);
  portalActive_ = !connected;

  if (portalActive_) {
    Serial.printf("Portal Wi-Fi: %s\n", setupAccessPointName_.c_str());
    Serial.printf("Senha do portal: %s\n", AppConfig::SETUP_AP_PASSWORD);
    Serial.printf("Abra http://%s apos conectar ao portal\n",
                  WiFi.softAPIP().toString().c_str());
  }

  handleConnectionChange(WiFi.status() == WL_CONNECTED);
}

void NetworkManager::update() {
  wifiManager_.process();

  const bool connected = WiFi.status() == WL_CONNECTED;
  if (connected != wasConnected_) {
    handleConnectionChange(connected);
  }

  if (resetPending_ &&
      static_cast<int32_t>(millis() - resetAtMs_) >= 0) {
    Serial.println("Apagando credenciais Wi-Fi e reiniciando...");
    wifiManager_.resetSettings();
    WiFi.disconnect(true, true);
    delay(100);
    ESP.restart();
  }
}

bool NetworkManager::isConnected() const {
  return WiFi.status() == WL_CONNECTED;
}

bool NetworkManager::isPortalActive() const { return portalActive_; }

String NetworkManager::ipAddress() const {
  return isConnected() ? WiFi.localIP().toString() : String("0.0.0.0");
}

int32_t NetworkManager::signalStrength() const {
  return isConnected() ? WiFi.RSSI() : 0;
}

const String& NetworkManager::setupAccessPointName() const {
  return setupAccessPointName_;
}

void NetworkManager::scheduleCredentialsReset() {
  resetPending_ = true;
  resetAtMs_ = millis() + 750;
}

void NetworkManager::handleConnectionChange(bool connected) {
  wasConnected_ = connected;

  if (!connected) {
    if (mdnsStarted_) {
      MDNS.end();
      mdnsStarted_ = false;
    }
    Serial.println("Wi-Fi desconectado; aguardando reconexao automatica");
    return;
  }

  if (portalActive_) {
    wifiManager_.stopConfigPortal();
    portalActive_ = false;
  }

  if (!mdnsStarted_) {
    mdnsStarted_ = MDNS.begin(AppConfig::HOSTNAME);
    if (mdnsStarted_) {
      MDNS.addService("http", "tcp", 80);
    }
  }

  Serial.printf("Wi-Fi conectado. IP: %s\n", WiFi.localIP().toString().c_str());
  Serial.printf("Interface: http://%s.local\n", AppConfig::HOSTNAME);
}
