#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

enum class OtaStatus : uint8_t {
  Idle,
  Checking,
  UpToDate,
  Available,
  Downloading,
  Verifying,
  Rebooting,
  Error,
};

struct OtaSnapshot {
  OtaStatus status = OtaStatus::Idle;
  char currentVersion[16] = {};
  char currentBuild[16] = {};
  char availableVersion[16] = {};
  char error[128] = {};
  uint32_t lastCheckEpoch = 0;
  uint8_t progress = 0;
  bool updateAvailable = false;
};

class OtaUpdateService {
 public:
  void begin();
  void markApplicationReady();
  void update(bool networkConnected, bool portalActive);
  bool requestCheck();
  bool requestInstall();
  bool prepareForRestart();
  void cancelRestartPreparation();
  void getSnapshot(OtaSnapshot& output) const;
  bool isBusy() const;
  static const char* statusName(OtaStatus status);

 private:
  enum class Operation : uint8_t { Check, Install };

  struct Manifest {
    char version[16] = {};
    char build[16] = {};
    char url[256] = {};
    char sha256[65] = {};
    size_t size = 0;
  };

  mutable portMUX_TYPE mutex_ = portMUX_INITIALIZER_UNLOCKED;
  OtaSnapshot snapshot_;
  Manifest manifest_;
  Preferences preferences_;
  bool preferencesReady_ = false;
  bool taskActive_ = false;
  bool wasConnected_ = false;
  bool healthConfirmed_ = false;
  bool pendingCleanup_ = false;
  bool applicationReady_ = false;
  bool restartPending_ = false;
  uint32_t connectedAtMs_ = 0;
  uint32_t pendingCleanupAttemptMs_ = 0;
  uint32_t lastCheckAtMs_ = 0;
  uint32_t applicationReadyAtMs_ = 0;
  uint32_t healthyLoopCycles_ = 0;
  Operation operation_ = Operation::Check;

  bool startOperation(Operation operation);
  static void taskEntry(void* context);
  void runOperation();
  bool checkForUpdate();
  bool fetchManifest(Manifest& manifest, char* error, size_t errorSize);
  bool install(const Manifest& manifest);
  bool synchronizeClock(char* error, size_t errorSize);
  void setStatus(OtaStatus status, uint8_t progress = 0);
  void setError(const char* message);
  void finishTask();
  void confirmHealthyFirmware();
  bool clearPendingVersion();
};
