#include "services/OtaUpdateService.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Update.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <esp_ota_ops.h>
#include <mbedtls/sha256.h>
#include <time.h>

#include "AppConfig.h"
#include "GitHubCertificates.h"

extern "C" bool verifyRollbackLater() { return true; }

namespace {
constexpr char OTA_NAMESPACE[] = "ota-update";
constexpr char KEY_PENDING_VERSION[] = "pending";
constexpr uint32_t VALID_EPOCH = 1700000000UL;
constexpr size_t DOWNLOAD_BUFFER_SIZE = 4096;

void copyText(char* target, size_t targetSize, const char* source) {
  strlcpy(target, source == nullptr ? "" : source, targetSize);
}

bool hasElapsed(uint32_t now, uint32_t since, uint32_t interval) {
  return static_cast<uint32_t>(now - since) >= interval;
}

bool parseVersion(const char* value, uint32_t parts[3]) {
  if (value == nullptr || *value == '\0') {
    return false;
  }

  const char* cursor = value;
  for (uint8_t index = 0; index < 3; ++index) {
    if (!isDigit(*cursor)) {
      return false;
    }
    if (*cursor == '0' && isDigit(*(cursor + 1))) {
      return false;
    }

    uint32_t part = 0;
    do {
      part = part * 10U + static_cast<uint32_t>(*cursor - '0');
      if (part > 9999U) {
        return false;
      }
      ++cursor;
    } while (isDigit(*cursor));
    parts[index] = part;

    if (index < 2) {
      if (*cursor != '.') {
        return false;
      }
      ++cursor;
    }
  }
  return *cursor == '\0';
}

int compareVersions(const char* left, const char* right) {
  uint32_t leftParts[3];
  uint32_t rightParts[3];
  if (!parseVersion(left, leftParts) || !parseVersion(right, rightParts)) {
    return 0;
  }
  for (uint8_t index = 0; index < 3; ++index) {
    if (leftParts[index] != rightParts[index]) {
      return leftParts[index] < rightParts[index] ? -1 : 1;
    }
  }
  return 0;
}

bool isSha256(const char* value) {
  if (value == nullptr || strlen(value) != 64) {
    return false;
  }
  for (size_t index = 0; index < 64; ++index) {
    if (!isDigit(value[index]) && (value[index] < 'a' || value[index] > 'f')) {
      return false;
    }
  }
  return true;
}
}  // namespace

void OtaUpdateService::begin() {
  copyText(snapshot_.currentVersion, sizeof(snapshot_.currentVersion),
           AppConfig::FIRMWARE_VERSION);
  copyText(snapshot_.currentBuild, sizeof(snapshot_.currentBuild),
           AppConfig::FIRMWARE_BUILD);

  preferencesReady_ = preferences_.begin(OTA_NAMESPACE, false);
  if (preferencesReady_) {
    const String pending = preferences_.getString(KEY_PENDING_VERSION, "");
    if (!pending.isEmpty() && pending != AppConfig::FIRMWARE_VERSION) {
      char message[128];
      snprintf(message, sizeof(message), "Atualizacao para %s foi revertida",
               pending.c_str());
      setError(message);
      pendingCleanup_ = !clearPendingVersion();
      pendingCleanupAttemptMs_ = millis();
    }
  }
}

void OtaUpdateService::markApplicationReady() {
  applicationReady_ = true;
  applicationReadyAtMs_ = millis();
  healthyLoopCycles_ = 0;
}

void OtaUpdateService::update(bool networkConnected, bool portalActive) {
  if (applicationReady_) {
    ++healthyLoopCycles_;
  }
  confirmHealthyFirmware();

  const uint32_t now = millis();
  if (networkConnected && !wasConnected_) {
    connectedAtMs_ = now;
  }
  wasConnected_ = networkConnected;

  if (!networkConnected || portalActive) {
    return;
  }

  portENTER_CRITICAL(&mutex_);
  const uint32_t lastCheckAtMs = lastCheckAtMs_;
  portEXIT_CRITICAL(&mutex_);
  const bool startupDue = lastCheckAtMs == 0 &&
                          hasElapsed(now, connectedAtMs_,
                                     AppConfig::OTA_STARTUP_CHECK_DELAY_MS);
  const bool periodicDue = lastCheckAtMs != 0 &&
                           hasElapsed(now, lastCheckAtMs,
                                      AppConfig::OTA_AUTO_CHECK_INTERVAL_MS);
  if (startupDue || periodicDue) {
    requestCheck();
  }
}

bool OtaUpdateService::requestCheck() {
  return startOperation(Operation::Check);
}

bool OtaUpdateService::requestInstall() {
  portENTER_CRITICAL(&mutex_);
  const bool available = snapshot_.updateAvailable;
  portEXIT_CRITICAL(&mutex_);
  return available && startOperation(Operation::Install);
}

bool OtaUpdateService::prepareForRestart() {
  portENTER_CRITICAL(&mutex_);
  if (taskActive_) {
    portEXIT_CRITICAL(&mutex_);
    return false;
  }
  restartPending_ = true;
  portEXIT_CRITICAL(&mutex_);
  return true;
}

void OtaUpdateService::cancelRestartPreparation() {
  portENTER_CRITICAL(&mutex_);
  restartPending_ = false;
  portEXIT_CRITICAL(&mutex_);
}

void OtaUpdateService::getSnapshot(OtaSnapshot& output) const {
  portENTER_CRITICAL(&mutex_);
  output = snapshot_;
  portEXIT_CRITICAL(&mutex_);
}

bool OtaUpdateService::isBusy() const {
  portENTER_CRITICAL(&mutex_);
  const bool busy = taskActive_;
  portEXIT_CRITICAL(&mutex_);
  return busy;
}

const char* OtaUpdateService::statusName(OtaStatus status) {
  switch (status) {
    case OtaStatus::Idle:
      return "idle";
    case OtaStatus::Checking:
      return "checking";
    case OtaStatus::UpToDate:
      return "up_to_date";
    case OtaStatus::Available:
      return "available";
    case OtaStatus::Downloading:
      return "downloading";
    case OtaStatus::Verifying:
      return "verifying";
    case OtaStatus::Rebooting:
      return "rebooting";
    case OtaStatus::Error:
      return "error";
  }
  return "error";
}

bool OtaUpdateService::startOperation(Operation operation) {
  portENTER_CRITICAL(&mutex_);
  if (taskActive_ || restartPending_) {
    portEXIT_CRITICAL(&mutex_);
    return false;
  }
  taskActive_ = true;
  operation_ = operation;
  snapshot_.status = OtaStatus::Checking;
  snapshot_.progress = 0;
  snapshot_.error[0] = '\0';
  portEXIT_CRITICAL(&mutex_);

  if (xTaskCreate(taskEntry, "ota-update", 12288, this, 1, nullptr) != pdPASS) {
    setError("Memoria insuficiente para iniciar a atualizacao");
    finishTask();
    return false;
  }
  return true;
}

void OtaUpdateService::taskEntry(void* context) {
  static_cast<OtaUpdateService*>(context)->runOperation();
  vTaskDelete(nullptr);
}

void OtaUpdateService::runOperation() {
  const Operation operation = operation_;
  const bool updateAvailable = checkForUpdate();
  if (operation == Operation::Install && updateAvailable) {
    Manifest manifest;
    portENTER_CRITICAL(&mutex_);
    manifest = manifest_;
    portEXIT_CRITICAL(&mutex_);
    install(manifest);
  }
  finishTask();
}

bool OtaUpdateService::checkForUpdate() {
  setStatus(OtaStatus::Checking);
  char error[128] = {};
  Manifest manifest;
  if (!fetchManifest(manifest, error, sizeof(error))) {
    portENTER_CRITICAL(&mutex_);
    lastCheckAtMs_ = millis();
    portEXIT_CRITICAL(&mutex_);
    setError(error);
    return false;
  }

  const bool available =
      compareVersions(AppConfig::FIRMWARE_VERSION, manifest.version) < 0;
  portENTER_CRITICAL(&mutex_);
  manifest_ = manifest;
  copyText(snapshot_.availableVersion, sizeof(snapshot_.availableVersion),
           manifest.version);
  snapshot_.lastCheckEpoch = static_cast<uint32_t>(time(nullptr));
  snapshot_.updateAvailable = available;
  snapshot_.status = available ? OtaStatus::Available : OtaStatus::UpToDate;
  snapshot_.progress = 0;
  snapshot_.error[0] = '\0';
  lastCheckAtMs_ = millis();
  portEXIT_CRITICAL(&mutex_);

  Serial.printf("OTA: versao instalada %s, versao publicada %s\n",
                AppConfig::FIRMWARE_VERSION, manifest.version);
  return available;
}

bool OtaUpdateService::fetchManifest(Manifest& manifest, char* error,
                                     size_t errorSize) {
  if (!WiFi.isConnected()) {
    copyText(error, errorSize, "Sem conexao com a internet");
    return false;
  }
  if (!synchronizeClock(error, errorSize)) {
    return false;
  }

  WiFiClientSecure client;
  client.setCACert(GITHUB_ROOT_CERTIFICATES);
  HTTPClient http;
  http.setConnectTimeout(AppConfig::OTA_HTTP_TIMEOUT_MS);
  http.setTimeout(AppConfig::OTA_HTTP_TIMEOUT_MS);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setRedirectLimit(5);
  http.setUserAgent("smart-abajour/" APP_VERSION);
  if (!http.begin(client, AppConfig::OTA_MANIFEST_URL)) {
    copyText(error, errorSize, "Nao foi possivel abrir o manifesto OTA");
    return false;
  }

  const int statusCode = http.GET();
  if (statusCode != HTTP_CODE_OK) {
    snprintf(error, errorSize, "GitHub respondeu HTTP %d", statusCode);
    http.end();
    return false;
  }
  const int contentLength = http.getSize();
  if (contentLength <= 0 ||
      contentLength > static_cast<int>(AppConfig::OTA_MAX_MANIFEST_BYTES)) {
    copyText(error, errorSize, "Tamanho do manifesto OTA invalido");
    http.end();
    return false;
  }

  StaticJsonDocument<768> document;
  const DeserializationError jsonError =
      deserializeJson(document, http.getStream());
  http.end();
  if (jsonError) {
    copyText(error, errorSize, "Manifesto OTA nao e um JSON valido");
    return false;
  }

  const int schema = document["schema"] | 0;
  const char* project = document["project"] | "";
  const char* target = document["target"] | "";
  const char* version = document["version"] | "";
  const char* build = document["build"] | "";
  const char* url = document["url"] | "";
  const char* sha256 = document["sha256"] | "";
  const size_t size = document["size"] | 0U;

  uint32_t versionParts[3];
  if (schema != 1 || strcmp(project, AppConfig::FIRMWARE_PROJECT) != 0 ||
      strcmp(target, AppConfig::FIRMWARE_TARGET) != 0 ||
      !parseVersion(version, versionParts) || size == 0 ||
      size > AppConfig::OTA_SLOT_SIZE || !isSha256(sha256) ||
      strlen(version) >= sizeof(manifest.version) ||
      strlen(build) >= sizeof(manifest.build) ||
      strlen(url) >= sizeof(manifest.url) ||
      strncmp(url, AppConfig::OTA_RELEASE_URL_PREFIX,
              strlen(AppConfig::OTA_RELEASE_URL_PREFIX)) != 0) {
    copyText(error, errorSize, "Manifesto OTA incompativel com este dispositivo");
    return false;
  }

  copyText(manifest.version, sizeof(manifest.version), version);
  copyText(manifest.build, sizeof(manifest.build), build);
  copyText(manifest.url, sizeof(manifest.url), url);
  copyText(manifest.sha256, sizeof(manifest.sha256), sha256);
  manifest.size = size;
  return true;
}

bool OtaUpdateService::install(const Manifest& manifest) {
  const esp_partition_t* running = esp_ota_get_running_partition();
  esp_ota_img_states_t runningState;
  const esp_err_t stateResult =
      esp_ota_get_state_partition(running, &runningState);
  if (stateResult == ESP_OK && runningState == ESP_OTA_IMG_PENDING_VERIFY) {
    setError("Aguarde a validacao do firmware atual antes de atualizar");
    return false;
  }
  if (stateResult != ESP_OK && stateResult != ESP_ERR_NOT_FOUND) {
    setError("Nao foi possivel validar o estado da particao atual");
    return false;
  }

  setStatus(OtaStatus::Downloading);
  WiFiClientSecure client;
  client.setCACert(GITHUB_ROOT_CERTIFICATES);
  HTTPClient http;
  http.setConnectTimeout(AppConfig::OTA_HTTP_TIMEOUT_MS);
  http.setTimeout(AppConfig::OTA_HTTP_TIMEOUT_MS);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setRedirectLimit(5);
  http.setUserAgent("smart-abajour/" APP_VERSION);
  if (!http.begin(client, manifest.url)) {
    setError("Nao foi possivel abrir o download do firmware");
    return false;
  }

  const int statusCode = http.GET();
  if (statusCode != HTTP_CODE_OK ||
      http.getSize() != static_cast<int>(manifest.size)) {
    char message[128];
    snprintf(message, sizeof(message), "Download OTA invalido (HTTP %d)",
             statusCode);
    setError(message);
    http.end();
    return false;
  }
  if (!Update.begin(manifest.size, U_FLASH)) {
    setError(Update.errorString());
    http.end();
    return false;
  }

  mbedtls_sha256_context shaContext;
  mbedtls_sha256_init(&shaContext);
  mbedtls_sha256_starts_ret(&shaContext, 0);
  uint8_t* buffer = static_cast<uint8_t*>(malloc(DOWNLOAD_BUFFER_SIZE));
  if (buffer == nullptr) {
    mbedtls_sha256_free(&shaContext);
    Update.abort();
    http.end();
    setError("Memoria insuficiente para baixar o firmware");
    return false;
  }

  WiFiClient& stream = http.getStream();
  size_t received = 0;
  uint32_t lastDataAtMs = millis();
  bool failed = false;
  while (received < manifest.size) {
    const int available = stream.available();
    if (available > 0) {
      const size_t requested = min(
          static_cast<size_t>(available),
          min(DOWNLOAD_BUFFER_SIZE, manifest.size - received));
      const size_t read = stream.readBytes(buffer, requested);
      if (read == 0 || Update.write(buffer, read) != read) {
        failed = true;
        break;
      }
      mbedtls_sha256_update_ret(&shaContext, buffer, read);
      received += read;
      lastDataAtMs = millis();
      setStatus(OtaStatus::Downloading,
                static_cast<uint8_t>((received * 100U) / manifest.size));
    } else if (!http.connected() ||
               hasElapsed(millis(), lastDataAtMs,
                          AppConfig::OTA_HTTP_TIMEOUT_MS)) {
      failed = true;
      break;
    } else {
      delay(1);
    }
  }
  free(buffer);
  http.end();

  uint8_t digest[32];
  mbedtls_sha256_finish_ret(&shaContext, digest);
  mbedtls_sha256_free(&shaContext);
  if (failed || received != manifest.size) {
    Update.abort();
    setError("Download do firmware foi interrompido");
    return false;
  }

  setStatus(OtaStatus::Verifying, 100);
  char actualSha256[65];
  for (size_t index = 0; index < sizeof(digest); ++index) {
    snprintf(actualSha256 + index * 2, 3, "%02x", digest[index]);
  }
  actualSha256[64] = '\0';
  if (strcmp(actualSha256, manifest.sha256) != 0) {
    Update.abort();
    setError("SHA-256 do firmware nao confere");
    return false;
  }
  if (!Update.end()) {
    setError(Update.errorString());
    return false;
  }

  if (preferencesReady_ &&
      preferences_.putString(KEY_PENDING_VERSION, manifest.version) == 0) {
    Serial.println("OTA: nao foi possivel registrar a versao pendente");
  }
  setStatus(OtaStatus::Rebooting, 100);
  Serial.printf("OTA: firmware %s validado; reiniciando\n", manifest.version);
  delay(1200);
  ESP.restart();
  return true;
}

bool OtaUpdateService::synchronizeClock(char* error, size_t errorSize) {
  if (time(nullptr) >= VALID_EPOCH) {
    return true;
  }
  configTime(0, 0, "pool.ntp.org", "time.google.com");
  const uint32_t startedAtMs = millis();
  while (time(nullptr) < VALID_EPOCH &&
         !hasElapsed(millis(), startedAtMs, 10000)) {
    delay(100);
  }
  if (time(nullptr) < VALID_EPOCH) {
    copyText(error, errorSize, "Nao foi possivel sincronizar o relogio");
    return false;
  }
  return true;
}

void OtaUpdateService::setStatus(OtaStatus status, uint8_t progress) {
  portENTER_CRITICAL(&mutex_);
  snapshot_.status = status;
  snapshot_.progress = progress;
  snapshot_.error[0] = '\0';
  portEXIT_CRITICAL(&mutex_);
}

void OtaUpdateService::setError(const char* message) {
  portENTER_CRITICAL(&mutex_);
  snapshot_.status = OtaStatus::Error;
  snapshot_.progress = 0;
  snapshot_.updateAvailable = false;
  snapshot_.availableVersion[0] = '\0';
  copyText(snapshot_.error, sizeof(snapshot_.error), message);
  portEXIT_CRITICAL(&mutex_);
  Serial.printf("OTA: %s\n", message);
}

void OtaUpdateService::finishTask() {
  portENTER_CRITICAL(&mutex_);
  taskActive_ = false;
  portEXIT_CRITICAL(&mutex_);
}

void OtaUpdateService::confirmHealthyFirmware() {
  if (pendingCleanup_ &&
      hasElapsed(millis(), pendingCleanupAttemptMs_, 1000)) {
    pendingCleanupAttemptMs_ = millis();
    pendingCleanup_ = !clearPendingVersion();
  }
  if (healthConfirmed_ || !applicationReady_ || healthyLoopCycles_ < 100 ||
      !hasElapsed(millis(), applicationReadyAtMs_,
                  AppConfig::OTA_HEALTH_CONFIRM_MS)) {
    return;
  }

  const esp_partition_t* running = esp_ota_get_running_partition();
  esp_ota_img_states_t state;
  if (esp_ota_get_state_partition(running, &state) != ESP_OK) {
    return;
  }
  if (state == ESP_OTA_IMG_PENDING_VERIFY) {
    if (esp_ota_mark_app_valid_cancel_rollback() != ESP_OK) {
      return;
    }
    Serial.println("OTA: firmware confirmado como saudavel");
    if (preferencesReady_) {
      pendingCleanup_ = !clearPendingVersion();
      pendingCleanupAttemptMs_ = millis();
    }
  }
  healthConfirmed_ = true;
}

bool OtaUpdateService::clearPendingVersion() {
  return !preferencesReady_ || !preferences_.isKey(KEY_PENDING_VERSION) ||
         preferences_.remove(KEY_PENDING_VERSION);
}
