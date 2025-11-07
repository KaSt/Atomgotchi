#ifndef AP_CONFIG_H
#define AP_CONFIG_H

#include "Arduino.h"
#include "WiFi.h"
#include "WebServer.h"
#include "config.h"

// AP Configuration
#define AP_SSID "Palnagotchi-Config"
#define AP_PASSWORD "palnagotchi"
#define AP_TIMEOUT_MS 300000  // 5 minutes

void initAPConfig();
void startAPMode();
void stopAPMode();
void handleAPConfig();
bool isAPModeActive();

#endif
