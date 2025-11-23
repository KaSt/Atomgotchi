#ifndef CONFIG_H
#define CONFIG_H

#include "Arduino.h"
#include "EEPROM.h"

// EEPROM memory layout
#define EEPROM_SIZE 512
#define EEPROM_CONFIG_MAGIC_ADDR   0       // 1 byte
#define EEPROM_FRIENDS_START_ADDR  4       // 8 bytes: 4–11
#define EEPROM_PWNED_START_ADDR    12      // 8 bytes: 12–19
#define EEPROM_EPOCH_START_ADDR    20      // 4 bytes: 20–23 (AI epoch counter)
#define EEPROM_CONFIG_START_ADDR   24      // config starts here
#define CONFIG_MAGIC_VALUE 0xAB

// Personality
#define FRIENDLY 0
#define AI 1

// Configuration structure
struct DeviceConfig {
  char device_name[32];
  uint8_t brightness;
  int personality;
  char* identity;
};

// Configuration functions
void initConfig();
void loadConfig();
void saveConfig();
void resetConfig();
DeviceConfig* getConfig();
String getDeviceName();
void setDeviceName(const char* name);
int getPersonality();
void setPersonality(const int personality);
String getPersonalityText();
void setIdentity(const char* identity );

void initStats();
void setStats(uint64_t _friends_tot, uint64_t _pwned_tot);
uint64_t getFriendsTot();
uint64_t getPwnedTot();
void setEpoch(uint32_t _epoch);
uint32_t getEpoch();

#endif
