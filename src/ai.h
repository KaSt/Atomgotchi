#ifndef AI_H
#define AI_H

#include "Arduino.h"
#include "nvs_flash.h"
#include "environment.h"
#include "action.h"

// Global environment accessor
Environment& getEnv();

// Time bucket helper (divides day into 4 periods)
inline int getTimeBucket() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return 1;  // Default to morning
  }
  
  int hour = timeinfo.tm_hour;
  if (hour < 6) return 0;       // Night (00:00-06:00)
  if (hour < 12) return 1;      // Morning (06:00-12:00)
  if (hour < 18) return 2;      // Afternoon (12:00-18:00)
  return 3;                     // Evening (18:00-24:00)
}

// State observation from environment
inline State State::fromObservation(const Environment& env) {
  State s;
  
  extern int wifi_get_channel();
  int ch = wifi_get_channel();
  s.channel = (ch - 1) % 13;
  
  // Classify AP density
  if (env.ap_count == 0) {
    s.ap_density = 0;      // Empty
  } else if (env.ap_count <= 2) {
    s.ap_density = 1;      // Low
  } else {
    s.ap_density = 2;      // High
  }
  
  s.recent_success = (env.got_handshake || env.got_pmkid) ? 1 : 0;
  s.time_bucket = getTimeBucket();
  s.epoch = 0;  // Set by agent
  
  return s;
}

void startBrain();
void stopBrain();

#endif // AI_H
