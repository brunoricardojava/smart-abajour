#pragma once

#include <Arduino.h>

#ifndef APP_VERSION
#define APP_VERSION "0.0.0"
#endif

#ifndef APP_BUILD
#define APP_BUILD "local"
#endif

namespace AppConfig {
constexpr uint8_t POT_PIN = 32;
constexpr uint8_t LED_PIN = 25;
constexpr uint8_t STATUS_LED_PIN = LED_BUILTIN;
constexpr bool STATUS_LED_ACTIVE_HIGH = true;
constexpr uint32_t STATUS_LED_UPDATE_INTERVAL_MS = 10;
constexpr uint8_t PWM_CHANNEL = 0;
constexpr uint32_t PWM_FREQUENCY_HZ = 5000;
constexpr uint8_t PWM_RESOLUTION_BITS = 12;
constexpr uint16_t PWM_MAX = (1U << PWM_RESOLUTION_BITS) - 1;
constexpr uint8_t ADC_ACTIVE_SAMPLES = 8;
constexpr uint8_t ADC_IDLE_SAMPLES = 4;
constexpr uint32_t ADC_ACTIVE_INTERVAL_MS = 20;
constexpr uint32_t ADC_IDLE_INTERVAL_MS = 40;
constexpr uint8_t POT_TAKEOVER_PERCENT = 2;
constexpr uint16_t POT_TAKEOVER_THRESHOLD =
    (static_cast<uint32_t>(PWM_MAX) * POT_TAKEOVER_PERCENT + 99U) / 100U;
constexpr uint8_t POT_TAKEOVER_CONFIRMATIONS = 2;
constexpr uint32_t SETTINGS_SAVE_DELAY_MS = 1000;
constexpr uint32_t SERIAL_INTERVAL_MS = 2000;

constexpr char FIRMWARE_VERSION[] = APP_VERSION;
constexpr char FIRMWARE_BUILD[] = APP_BUILD;
constexpr char FIRMWARE_PROJECT[] = "smart-abajour";
constexpr char FIRMWARE_TARGET[] = "mhetesp32minikit";
constexpr char OTA_MANIFEST_URL[] =
    "https://github.com/brunoricardojava/smart-abajour/releases/latest/"
    "download/manifest.json";
constexpr char OTA_RELEASE_URL_PREFIX[] =
    "https://github.com/brunoricardojava/smart-abajour/releases/download/";
constexpr uint32_t OTA_AUTO_CHECK_INTERVAL_MS = 24UL * 60UL * 60UL * 1000UL;
constexpr uint32_t OTA_STARTUP_CHECK_DELAY_MS = 5000;
constexpr uint32_t OTA_HEALTH_CONFIRM_MS = 15000;
constexpr uint32_t APPLICATION_WATCHDOG_TIMEOUT_MS = 30000;
constexpr uint32_t APPLICATION_WATCHDOG_FEED_INTERVAL_MS = 250;
constexpr uint32_t LOOP_IDLE_DELAY_MS = 4;
constexpr uint32_t OTA_HTTP_TIMEOUT_MS = 15000;
constexpr size_t OTA_MAX_MANIFEST_BYTES = 2048;
constexpr size_t OTA_SLOT_SIZE = 0x140000;

constexpr char HOSTNAME[] = "esp32-led";
constexpr char SETUP_AP_PREFIX[] = "CONFIGURE-LED";
constexpr char SETUP_AP_PASSWORD[] = "configure-led";
constexpr uint16_t WIFI_CONNECT_TIMEOUT_SECONDS = 15;

static_assert(STATUS_LED_PIN != POT_PIN,
              "Status LED must not share the potentiometer pin");
static_assert(STATUS_LED_PIN != LED_PIN,
              "Status LED must not share the controlled LED pin");
}  // namespace AppConfig
