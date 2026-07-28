#include <Arduino.h>

#include "AppConfig.h"
#include "AppState.h"
#include "controllers/LedController.h"
#include "services/NetworkManager.h"
#include "services/OtaUpdateService.h"
#include "services/SettingsStore.h"
#include "services/StatusIndicator.h"
#include "services/WebServerService.h"

namespace {
AppState appState;
LedController ledController(appState);
NetworkManager networkManager;
OtaUpdateService otaUpdateService;
SettingsStore settingsStore;
StatusIndicator statusIndicator;
WebServerService webServer(appState, networkManager, settingsStore,
                           otaUpdateService);
portMUX_TYPE watchdogMutex = portMUX_INITIALIZER_UNLOCKED;
uint32_t watchdogHeartbeatMs = 0;

void applicationWatchdogTask(void*) {
  while (true) {
    delay(1000);
    portENTER_CRITICAL(&watchdogMutex);
    const uint32_t heartbeat = watchdogHeartbeatMs;
    portEXIT_CRITICAL(&watchdogMutex);
    if (static_cast<uint32_t>(millis() - heartbeat) >=
        AppConfig::APPLICATION_WATCHDOG_TIMEOUT_MS) {
      ESP.restart();
    }
  }
}

bool startApplicationWatchdog() {
  watchdogHeartbeatMs = millis();
  if (xTaskCreate(applicationWatchdogTask, "app-watchdog", 2048, nullptr, 2,
                  nullptr) != pdPASS) {
    Serial.println("Falha ao iniciar watchdog da aplicacao");
    return false;
  }
  return true;
}

void feedApplicationWatchdog() {
  static uint32_t lastFeedAtMs = 0;
  const uint32_t now = millis();
  if (now - lastFeedAtMs <
      AppConfig::APPLICATION_WATCHDOG_FEED_INTERVAL_MS) {
    return;
  }
  lastFeedAtMs = now;

  portENTER_CRITICAL(&watchdogMutex);
  watchdogHeartbeatMs = now;
  portEXIT_CRITICAL(&watchdogMutex);
}
}  // namespace

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("Iniciando controle inteligente do LED");
  statusIndicator.begin();
  if (!startApplicationWatchdog()) {
    delay(1000);
    ESP.restart();
  }

  settingsStore.begin(appState);
  ledController.begin();
  otaUpdateService.begin();
  networkManager.begin();
  ledController.calibrateTouch();
  otaUpdateService.markApplicationReady();
}

void loop() {
  networkManager.update();
  otaUpdateService.update(networkManager.isConnected(),
                          networkManager.isPortalActive());

  if (networkManager.isConnected() && !webServer.isStarted()) {
    webServer.begin();
  }

  webServer.update();
  if (ledController.update()) {
    settingsStore.scheduleSave();
  }
  settingsStore.update(appState);

  const uint32_t now = millis();
  static uint32_t lastStatusIndicatorUpdateMs = 0;
  if (now - lastStatusIndicatorUpdateMs >=
      AppConfig::STATUS_LED_UPDATE_INTERVAL_MS) {
    lastStatusIndicatorUpdateMs = now;
    OtaSnapshot otaState;
    otaUpdateService.getSnapshot(otaState);
    statusIndicator.update(now, networkManager.isConnected(),
                           networkManager.isPortalActive(),
                           networkManager.isResetPending(), otaState.status);
  }
  feedApplicationWatchdog();

  static uint32_t lastSerialUpdateMs = 0;
  if (now - lastSerialUpdateMs >= AppConfig::SERIAL_INTERVAL_MS) {
    lastSerialUpdateMs = now;
    Serial.printf(
        "Modo: %s | ADC: %u | Saida: %u%% | Touch: %u/%u | Wi-Fi: %s\n",
                  appState.controlMode == ControlMode::Potentiometer ? "pot"
                                                                     : "web",
                  appState.potentiometerAdc, appState.outputBrightness,
                  ledController.touchValue(), ledController.touchBaseline(),
                  networkManager.isConnected() ? "conectado" : "configurando");
  }

  delay(AppConfig::LOOP_IDLE_DELAY_MS);
}
