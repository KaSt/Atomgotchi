#ifndef AP_CONFIG_H
#define AP_CONFIG_H

#include "Arduino.h"
#include "M5Unified.h"
#include "WiFi.h"
#include "esp_wifi.h"
#include "WebServer.h"
#include "config.h"

using namespace std;


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
