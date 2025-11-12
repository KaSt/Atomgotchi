#include "esp_wifi.h"
#include "esp_err.h"

#include "config.h"

DeviceConfig device_config;

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
    case PASSIVE:
      return "P";
      break;
    case AGGRESSIVE:
      return "A";
      break;
    default:
      return "F";
      break;
  }
}

void setPersonality(int personality) {
  Serial.println("Setting personality...");
  if (personality != FRIENDLY && personality != PASSIVE && personality != AGGRESSIVE) {
    Serial.println("Wrong Personality.");
    return;
  }
  device_config.personality = personality;
  saveConfig();
  Serial.println("setPersonality done.");
}
