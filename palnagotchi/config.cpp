#include "esp_wifi.h"
#include "esp_err.h"

#include "config.h"
#include <Preferences.h>

DeviceConfig device_config;
static String s_identity;  // storage interno sicuro

void initStats() {
  uint64_t friends_tot = 0;
  uint64_t pwned_tot = 0;
  EEPROM.writeLong64(EEPROM_FRIENDS_START_ADDR, friends_tot);
  EEPROM.writeLong64(EEPROM_PWNED_START_ADDR, pwned_tot);
  EEPROM.commit();
}

void setStats(uint64_t _friends_tot, uint64_t _pwned_tot) {
  EEPROM.writeLong64(EEPROM_FRIENDS_START_ADDR, _friends_tot);
  EEPROM.writeLong64(EEPROM_PWNED_START_ADDR, _pwned_tot);
  EEPROM.commit();
}

uint64_t getFriendsTot() {
  return EEPROM.readLong64(EEPROM_FRIENDS_START_ADDR);
}

uint64_t getPwnedTot() {
  return EEPROM.readLong64(EEPROM_PWNED_START_ADDR);
}

void initConfig() {
  Serial.println("initConfig...");
  EEPROM.begin(EEPROM_SIZE);
  loadConfig();
  Serial.println("initConfig done.");
}

void loadConfig() {
  Serial.println("loadConfig...");
  uint8_t magic = EEPROM.read(EEPROM_CONFIG_MAGIC_ADDR);
  
  if (magic == CONFIG_MAGIC_VALUE) {
    // Load configuration from EEPROM
    EEPROM.get(EEPROM_CONFIG_START_ADDR, device_config);
  } else {
    // First time setup - initialize with defaults
    initStats();
    resetConfig();
    saveConfig();
  }
  Serial.println("loadConfig done.");
}

void saveConfig() {
  Serial.println("saveConfig...");
  // Write magic value
  EEPROM.write(EEPROM_CONFIG_MAGIC_ADDR, CONFIG_MAGIC_VALUE);
  // Write configuration
  EEPROM.put(EEPROM_CONFIG_START_ADDR, device_config);
  EEPROM.commit();
  Serial.println("saveConfig done.");
}

void resetConfig() {
  Serial.println("resetConfig...");
  strncpy(device_config.device_name, "Gotchi", sizeof(device_config.device_name) - 1);
  device_config.device_name[sizeof(device_config.device_name) - 1] = '\0';
  device_config.brightness = 128;
  device_config.personality = FRIENDLY;
  Serial.println("resetConfig done.");
}

DeviceConfig* getConfig() {
  return &device_config;
}

String getDeviceName() {
  return String(device_config.device_name);
}

void setDeviceName(const char* name) {
  Serial.println("setDeviceName...");
  strncpy(device_config.device_name, name, sizeof(device_config.device_name) - 1);
  device_config.device_name[sizeof(device_config.device_name) - 1] = '\0';
  saveConfig();
  Serial.println("setDeviceName done.");
}

int getPersonality() {
  return int(device_config.personality);
}

String getPersonalityText() {
  String pers;
  switch (device_config.personality) {
    case AI:
      return "A";
      break;
    default:
      return "F";
      break;
  }
}

void setPersonality(int personality) {
  Serial.println("Setting personality...");
  if (personality != FRIENDLY 
                  && personality != AI) {
    Serial.println("Wrong Personality.");
    return;
  }
  
  device_config.personality = personality;
  saveConfig();
  Serial.println("setPersonality done.");
}




