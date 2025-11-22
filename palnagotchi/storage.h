#ifndef STORAGE_H
#define STORAGE_H

#include <Arduino.h>
#include <FS.h>
#include <LittleFS.h>
#include <SD.h>

// Storage preference
enum StorageType {
    STORAGE_NONE = 0,
    STORAGE_LITTLEFS = 1,
    STORAGE_SD = 2
};

class StorageManager {
private:
    StorageType currentStorage;
    bool sdAvailable;
    bool sdFormatted;
    bool littleFSAvailable;
    
    // SD card pins for different devices
    int sdCSPin;
    int sdMOSIPin;
    int sdMISOPin;
    int sdSCKPin;
    
    bool detectSDCard();
    bool checkSDFormatted();
    bool migrateDataToSD();
    bool migrateDataToLittleFS();
    void autoDetectSDPins();
    
public:
    StorageManager();
    
    bool begin();
    bool formatSD();
    bool switchToSD();
    bool switchToLittleFS();
    
    // Get the active filesystem
    fs::FS& getFS();
    
    // Status queries
    bool isSDAvailable() const { return sdAvailable; }
    bool isSDFormatted() const { return sdFormatted; }
    bool isSDActive() const { return currentStorage == STORAGE_SD; }
    StorageType getStorageType() const { return currentStorage; }
    const char* getStorageTypeName() const;
    
    // File operations using active storage
    File open(const char* path, const char* mode);
    bool exists(const char* path);
    bool remove(const char* path);
    bool rename(const char* pathFrom, const char* pathTo);
    
    // SD card info
    uint64_t getSDCardSize();
    uint64_t getSDCardUsed();
    uint64_t getSDCardFree();
};

// Global storage manager
extern StorageManager storage;

#endif // STORAGE_H
