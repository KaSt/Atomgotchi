#include "config.h"

DeviceConfig device_config;

void initConfig() {
  EEPROM.begin(EEPROM_SIZE);
  loadConfig();
}

void loadConfig() {
  uint8_t magic = EEPROM.read(EEPROM_CONFIG_MAGIC_ADDR);
  
  if (magic == CONFIG_MAGIC_VALUE) {
    // Load configuration from EEPROM
    EEPROM.get(EEPROM_CONFIG_START_ADDR, device_config);
  } else {
    // First time setup - initialize with defaults
    resetConfig();
    saveConfig();
  }
}

void saveConfig() {
  // Write magic value
  EEPROM.write(EEPROM_CONFIG_MAGIC_ADDR, CONFIG_MAGIC_VALUE);
  // Write configuration
  EEPROM.put(EEPROM_CONFIG_START_ADDR, device_config);
  EEPROM.commit();
}

void resetConfig() {
  strncpy(device_config.device_name, "Palnagotchi", sizeof(device_config.device_name) - 1);
  device_config.device_name[sizeof(device_config.device_name) - 1] = '\0';
  device_config.brightness = 128;
  device_config.sound_enabled = false;
}

DeviceConfig* getConfig() {
  return &device_config;
}

String getDeviceName() {
  return String(device_config.device_name);
}

void setDeviceName(const char* name) {
  strncpy(device_config.device_name, name, sizeof(device_config.device_name) - 1);
  device_config.device_name[sizeof(device_config.device_name) - 1] = '\0';
  saveConfig();
}
