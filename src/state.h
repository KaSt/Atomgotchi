#ifndef STATE_H
#define STATE_H

#include "Arduino.h"
#include "M5Unified.h"
#include "WiFi.h"
#include "esp_wifi.h"
#include "environment.h"

// Forward declaration
int getHour();

// State representation
struct State {
  int channel;           // 0-12 (WiFi channels 1-13)
  int ap_density;        // 0=empty, 1=low (1-2), 2=high (3+)
  int recent_success;    // 0=no, 1=yes (handshake/PMKID in last N epochs)
  int signal_strength;   // 0=weak (<-70dBm), 1=medium (-70 to -50), 2=strong (>-50dBm)
  int station_activity;  // 0=no stations, 1=few (1-3), 2=many (4+)
  uint32_t epoch;        // Current training epoch (for display only)

  // Convert state to index for Q-table lookup
  int toIndex() const {
    // State space: 13 channels × 3 densities × 2 success × 3 signals × 3 stations = 702 states
    return channel * 54 + ap_density * 18 + recent_success * 9 + signal_strength * 3 + station_activity;
  }

  // Create state from environment observation
  static State fromObservation(const Environment& env);
};

// Implementation of State::fromObservation
inline State State::fromObservation(const Environment& env) {
  State s;
  
  // Get current WiFi channel (you need to implement wifi_get_channel())
  extern int wifi_get_channel();
  int current_ch = wifi_get_channel();
  s.channel = (current_ch - 1) % 13;  // Normalize to 0-12
  
  // Classify AP density
  if (env.ap_count == 0) {
    s.ap_density = 0;  // Empty
  } else if (env.ap_count <= 2) {
    s.ap_density = 1;  // Low density
  } else {
    s.ap_density = 2;  // High density
  }
  
  // Recent success flag
  s.recent_success = (env.got_handshake || env.got_pmkid) ? 1 : 0;
  
  // Composite "Target Quality" score
  // Combines signal strength + station activity + encryption type
  int quality_score = 0;
  
  // Signal strength contribution (0-2 points)
  if (env.strongest_rssi > -50) {
    quality_score += 2;  // Strong signal
  } else if (env.strongest_rssi > -70) {
    quality_score += 1;  // Medium signal
  }
  // else: weak signal = 0 points
  
  // Station activity contribution (0-2 points)
  if (env.station_count > 3) {
    quality_score += 2;  // Busy network
  } else if (env.station_count > 0) {
    quality_score += 1;  // Some activity
  }
  // else: no stations = 0 points
  
  // Encryption type contribution (0-1 point)
  if (env.wpa2_ratio > 0.5f) {
    quality_score += 1;  // Mostly WPA2 networks (our target)
  }
  
  // Bucket into 3 quality levels (0-5 points total possible)
  if (quality_score >= 4) {
    s.target_quality = 2;  // Excellent target (4-5 points)
  } else if (quality_score >= 2) {
    s.target_quality = 1;  // Okay target (2-3 points)
  } else {
    s.target_quality = 0;  // Poor target (0-1 points)
  }
  
  s.epoch = 0;  // Will be set by agent
  
  return s;
}

// Actions
enum Action {
    STAY_CHANNEL = 0,      // Keep monitoring current channel
    NEXT_CHANNEL = 1,      // Hop to next channel
    PREV_CHANNEL = 2,      // Hop to previous channel
    AGGRESSIVE_MODE = 3,       // Aggressive: send deauth + monitor
    IDLE_MODE = 4,         // Power save mode
    NUM_ACTIONS = 5
};

#endif // STATE_H