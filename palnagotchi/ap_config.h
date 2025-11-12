#ifndef AP_CONFIG_H
#define AP_CONFIG_H

#include "Arduino.h"
#include "ArduinoJson.h"
#include "M5Unified.h"
#include "WiFi.h"
#include "esp_wifi.h"
#include "WebServer.h"
#include "LittleFS.h"
#include "mbedtls/base64.h"   // per decodificare base64 (ESP-IDF)
#include "config.h"

using namespace std;

#ifndef FR_TBL
#define FR_TBL "/friends.ndjson"
#endif

#ifndef PK_TBL
#define PK_TBL "/packets.ndjson"
#endif

// AP Configuration
#define AP_SSID "Gotchi"
#define AP_PASSWORD "GotchiPass"
#define AP_TIMEOUT_MS 300000  // 5 minutes

void initAPConfig();
void startAPMode();
void stopAPMode();
void handleAPConfig();
bool isAPModeActive();
bool shouldExitAPMode();

#endif
