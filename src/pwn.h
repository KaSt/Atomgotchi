#ifndef _PWN_H_
#define _PWN_H_

#include <vector>
#include <set>
#include "ArduinoJson.h"
#include "esp_wifi.h"
#include "esp_wifi_types.h"
#include "freertos/FreeRTOS.h"
#include "pwnagotchi.h"
#include "environment.h"
#include "config.h"
#include "db.h"

#define GRID_VERSION    "1.10.3"
#define PWNGRID_VERSION "1.8.4"
#define MAX_PKT_SAVE    800

extern void drawMood(String face, String phrase, bool broken,
                      String last_friend_name, signed int rssi);

struct BeaconEntry {
    uint8_t mac[6]{};
    uint8_t channel{0};

    bool operator<(const BeaconEntry &other) const {
        int cmp = memcmp(mac, other.mac, sizeof(mac));
        if (cmp != 0) { return cmp < 0; }
        return channel < other.channel;
    }
};

// WiFi channel management
int wifi_get_channel();
void wifi_set_channel(int ch);

// Pwngrid system
void initPwning();
esp_err_t pwngridAdvertise(uint8_t channel, String face);
pwngrid_peer* getPwngridPeers();
uint64_t getPwngridRunTotalPeers();
uint64_t getPwngridTotalPeers();
String getPwngridLastFriendName();
signed int getPwngridClosestRssi();
void checkPwngridGoneFriends();
uint64_t getPwngridRunPwned();
uint64_t getPwngridTotalPwned();

// Attack operations
void performDeauthCycle();

// GPS functions
bool hasGPSModule();
bool isGPSConnected();
bool hasGPSFix();
void getGPSCoordinates(double &lat, double &lon);

// Environment
Environment &getEnv();

#endif
