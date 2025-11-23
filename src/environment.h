#ifndef ENVIRONMENT_H
#define ENVIRONMENT_H

#include "Arduino.h"
#include "M5Unified.h"
#include "WiFi.h"
#include "esp_wifi.h"
#include <set>
#include "action.h"

struct Environment;

// State representation
struct State {
  int channel;           // 0-12 (WiFi channels 1-13)
  int ap_density;        // 0=empty, 1=low (1-2), 2=high (3+)
  int recent_success;    // 0=no, 1=yes (handshake/PMKID in last N epochs)
  int time_bucket;       // 0=night, 1=morning, 2=afternoon, 3=evening
  uint32_t epoch;        // Current training epoch (for display only)

  // Convert state to index for Q-table lookup
  int toIndex() const {
    // State space: 13 channels × 3 densities × 2 success × 4 time buckets = 312 states
    return channel * 24 + ap_density * 8 + recent_success * 4 + time_bucket;
  }

  // Create state from environment observation
  static State fromObservation(const Environment& env);
};

// Forward declarations of helper functions
uint64_t getMacFromPacket(const wifi_promiscuous_pkt_t* pkt);
bool isEAPOL(const wifi_promiscuous_pkt_t* pkt);
bool isCompleteHandshake(const wifi_promiscuous_pkt_t* pkt);
bool hasPMKID(const wifi_promiscuous_pkt_t* pkt);

// Environment contains all observable data
struct Environment {
  int channel;
  int ap_count;              // Number of APs on current channel
  int new_aps_found;         // New APs discovered this step
  int eapol_packets;         // EAPOL packets seen (handshake indicators)
  bool got_handshake;        // Captured WPA handshake this step
  bool got_pmkid;            // Captured PMKID this step
  unsigned long time_on_channel;  // Milliseconds on current channel
  unsigned long idle_time;   // Time spent in idle mode
  unsigned long channel_start_time; //Since when on channel
  unsigned long last_handshake_time;
  Action action;
  
  // Quality indicators
  int strongest_rssi;        // RSSI of strongest AP (-30 to -90)
  int station_count;         // Number of clients/stations seen
  float wpa2_ratio;          // Ratio of WPA2 APs (0.0 to 1.0)
  
  void reset() {
    uint8_t primary;
    wifi_second_chan_t second;
    esp_wifi_get_channel(&primary, &second);
    if (primary != channel) {
        channel_start_time = millis();
        channel = primary;
        station_count = 0;
        time_on_channel = 0;
    }
    new_aps_found = 0;
    eapol_packets = 0;
    got_handshake = false;
    got_pmkid = false;
    idle_time = 0;
    strongest_rssi = -100;
    wpa2_ratio = 0.0f;
  }

  void update() {
    time_on_channel = millis() - channel_start_time;
  }
};


#endif // ENVIRONMENT_H