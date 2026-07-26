#include "services/WebServerService.h"

#include <cstdlib>
#include <esp_system.h>

#include "AppConfig.h"
#include "services/NetworkManager.h"
#include "services/OtaUpdateService.h"
#include "services/SettingsStore.h"
#include "web/WebPage.h"

namespace {
bool parseBoolean(const String& value, bool& result) {
  if (value == "true" || value == "1") {
    result = true;
    return true;
  }
  if (value == "false" || value == "0") {
    result = false;
    return true;
  }
  return false;
}

bool parseBrightness(const String& value, uint8_t& result) {
  if (value.isEmpty()) {
    return false;
  }

  for (size_t index = 0; index < value.length(); ++index) {
    if (!isDigit(value[index])) {
      return false;
    }
  }

  const long parsed = strtol(value.c_str(), nullptr, 10);
  if (parsed < 0 || parsed > 100) {
    return false;
  }

  result = static_cast<uint8_t>(parsed);
  return true;
}
}  // namespace

WebServerService::WebServerService(AppState& state, NetworkManager& network,
                                   SettingsStore& settings,
                                   OtaUpdateService& ota)
    : state_(state), network_(network), settings_(settings), ota_(ota) {}

void WebServerService::begin() {
  if (started_) {
    return;
  }

  snprintf(csrfToken_, sizeof(csrfToken_), "%08lx%08lx",
           static_cast<unsigned long>(esp_random()),
           static_cast<unsigned long>(esp_random()));
  const char* collectedHeaders[] = {"X-CSRF-Token"};
  server_.collectHeaders(collectedHeaders, 1);

  server_.on("/", HTTP_GET, [this]() {
    server_.sendHeader("Cache-Control", "no-cache");
    server_.send_P(200, "text/html; charset=utf-8", WEB_PAGE);
  });
  server_.on("/api/state", HTTP_GET, [this]() { handleState(); });
  server_.on("/api/control", HTTP_POST, [this]() { handleControl(); });
  server_.on("/api/wifi/reset", HTTP_POST,
             [this]() { handleWifiReset(); });
  server_.on("/api/factory-reset", HTTP_POST,
             [this]() { handleFactoryReset(); });
  server_.on("/api/ota/check", HTTP_POST,
             [this]() { handleOtaCheck(); });
  server_.on("/api/ota/install", HTTP_POST,
             [this]() { handleOtaInstall(); });
  server_.onNotFound([this]() {
    if (server_.uri().startsWith("/api/")) {
      sendError(404, F("Endpoint nao encontrado"));
      return;
    }
    server_.sendHeader("Location", "/", true);
    server_.send(302, "text/plain", "");
  });

  server_.begin();
  started_ = true;
  Serial.println("Servidor web iniciado na porta 80");
}

void WebServerService::update() {
  if (started_) {
    server_.handleClient();
  }
}

bool WebServerService::isStarted() const { return started_; }

void WebServerService::handleState() { sendState(); }

void WebServerService::handleControl() {
  if (!authorizeMutation()) {
    return;
  }
  bool nextPower = state_.powerOn;
  ControlMode nextMode = state_.controlMode;
  uint8_t nextBrightness = state_.manualBrightness;
  bool hasControl = false;
  bool hasWebInteraction = false;

  if (server_.hasArg("power")) {
    hasControl = true;
    if (!parseBoolean(server_.arg("power"), nextPower)) {
      sendError(400, F("Valor de power invalido"));
      return;
    }
  }

  if (server_.hasArg("brightness")) {
    hasControl = true;
    hasWebInteraction = true;
    if (!parseBrightness(server_.arg("brightness"), nextBrightness)) {
      sendError(400, F("Brightness deve estar entre 0 e 100"));
      return;
    }
    nextMode = ControlMode::Web;
    nextPower = true;
  }

  if (!hasControl) {
    sendError(400, F("Nenhum controle foi informado"));
    return;
  }

  const bool changed = nextPower != state_.powerOn ||
                       nextMode != state_.controlMode ||
                       nextBrightness != state_.manualBrightness;
  state_.powerOn = nextPower;
  state_.controlMode = nextMode;
  state_.manualBrightness = nextBrightness;
  if (hasWebInteraction) {
    ++state_.webInteractionRevision;
  }
  state_.outputBrightness =
      state_.powerOn
          ? (state_.controlMode == ControlMode::Potentiometer
                 ? state_.potentiometerBrightness
                 : state_.manualBrightness)
          : 0;

  if (changed) {
    settings_.scheduleSave();
  }
  sendState();
}

void WebServerService::handleWifiReset() {
  if (!authorizeMutation()) {
    return;
  }
  if (!ota_.prepareForRestart()) {
    sendError(409, F("Aguarde a operacao OTA terminar"));
    return;
  }
  server_.send(202, "application/json",
               "{\"ok\":true,\"message\":\"Reiniciando no modo de configuracao\"}");
  network_.scheduleCredentialsReset();
}

void WebServerService::handleFactoryReset() {
  if (!authorizeMutation()) {
    return;
  }
  if (!ota_.prepareForRestart()) {
    sendError(409, F("Aguarde a operacao OTA terminar"));
    return;
  }
  if (!settings_.restoreFactoryDefaults(state_)) {
    ota_.cancelRestartPreparation();
    sendError(500, F("Nao foi possivel apagar as preferencias"));
    return;
  }

  server_.send(
      202, "application/json",
      "{\"ok\":true,\"message\":\"Configuracoes de fabrica restauradas\"}");
  network_.scheduleCredentialsReset();
}

void WebServerService::handleOtaCheck() {
  if (!authorizeMutation()) {
    return;
  }
  if (!network_.isConnected()) {
    sendError(503, F("Sem conexao com a internet"));
    return;
  }
  if (!ota_.requestCheck()) {
    sendError(409, F("Uma operacao OTA ja esta em andamento"));
    return;
  }
  server_.send(202, "application/json",
               "{\"ok\":true,\"message\":\"Consulta OTA iniciada\"}");
}

void WebServerService::handleOtaInstall() {
  if (!authorizeMutation()) {
    return;
  }
  OtaSnapshot otaState;
  ota_.getSnapshot(otaState);
  if (!otaState.updateAvailable) {
    sendError(409, F("Nenhuma atualizacao esta disponivel"));
    return;
  }
  if (!ota_.requestInstall()) {
    sendError(409, F("Uma operacao OTA ja esta em andamento"));
    return;
  }
  server_.send(202, "application/json",
               "{\"ok\":true,\"message\":\"Atualizacao OTA iniciada\"}");
}

bool WebServerService::authorizeMutation() {
  if (server_.header("X-CSRF-Token") == csrfToken_) {
    return true;
  }
  sendError(403, F("Token de seguranca ausente ou invalido"));
  return false;
}

void WebServerService::sendState() {
  OtaSnapshot otaState;
  ota_.getSnapshot(otaState);
  String json;
  json.reserve(700);
  json += F("{\"power\":");
  json += state_.powerOn ? F("true") : F("false");
  json += F(",\"mode\":\"");
  json += state_.controlMode == ControlMode::Potentiometer
              ? F("potentiometer")
              : F("web");
  json += F("\",\"manualBrightness\":");
  json += state_.manualBrightness;
  json += F(",\"potentiometerBrightness\":");
  json += state_.potentiometerBrightness;
  json += F(",\"potentiometerAdc\":");
  json += state_.potentiometerAdc;
  json += F(",\"outputBrightness\":");
  json += state_.outputBrightness;
  json += F(",\"wifiConnected\":");
  json += network_.isConnected() ? F("true") : F("false");
  json += F(",\"portalActive\":");
  json += network_.isPortalActive() ? F("true") : F("false");
  json += F(",\"rssi\":");
  json += network_.signalStrength();
  json += F(",\"ip\":\"");
  json += network_.ipAddress();
  json += F("\",\"hostname\":\"");
  json += AppConfig::HOSTNAME;
  json += F(".local\",\"firmwareVersion\":\"");
  json += otaState.currentVersion;
  json += F("\",\"firmwareBuild\":\"");
  json += otaState.currentBuild;
  json += F("\",\"otaStatus\":\"");
  json += OtaUpdateService::statusName(otaState.status);
  json += F("\",\"otaAvailableVersion\":\"");
  json += otaState.availableVersion;
  json += F("\",\"otaProgress\":");
  json += otaState.progress;
  json += F(",\"otaLastCheck\":");
  json += otaState.lastCheckEpoch;
  json += F(",\"otaUpdateAvailable\":");
  json += otaState.updateAvailable ? F("true") : F("false");
  json += F(",\"otaError\":\"");
  json += otaState.error;
  json += F("\",\"csrfToken\":\"");
  json += csrfToken_;
  json += F("\"}");

  server_.sendHeader("Cache-Control", "no-store");
  server_.send(200, "application/json", json);
}

void WebServerService::sendError(
    uint16_t statusCode, const __FlashStringHelper* message) {
  String json = F("{\"ok\":false,\"error\":\"");
  json += message;
  json += F("\"}");
  server_.send(statusCode, "application/json", json);
}
