#include "storage.h"
#include <Preferences.h>

// Global instance
StorageManager storage;

// Preferences for storing user choice
Preferences storagePrefs;

StorageManager::StorageManager() {
    currentStorage = STORAGE_NONE;
    sdAvailable = false;
    sdFormatted = false;
    littleFSAvailable = false;
    sdCSPin = -1;
    sdMOSIPin = -1;
    sdMISOPin = -1;
    sdSCKPin = -1;
}

void StorageManager::autoDetectSDPins() {
    // Auto-detect SD card pins based on device
    #if defined(ARDUINO_M5STACK_ATOMS3)
        // Atomic GPS Base has SD card
        // Check standard M5Atom pins
        sdCSPin = 5;    // Typical CS pin for Atomic GPS
        sdMOSIPin = 23;
        sdMISOPin = 19;
        sdSCKPin = 18;
        Serial.println("SD: Detected Atom S3 - using Atomic GPS pins");
    #elif defined(ARDUINO_M5STACK_STICKC_PLUS) || defined(ARDUINO_M5STACK_STICKC_PLUS2)
        // Some StickC Plus accessories have SD
        sdCSPin = 4;
        sdMOSIPin = 23;
        sdMISOPin = 19;
        sdSCKPin = 18;
        Serial.println("SD: Detected StickC Plus - checking for SD hat");
    #elif defined(ARDUINO_M5STACK_CARDPUTER)
        // Cardputer may have SD slot on some versions
        sdCSPin = 12;
        sdMOSIPin = 13;
        sdMISOPin = 11;
        sdSCKPin = 14;
        Serial.println("SD: Detected Cardputer - checking for SD");
    #else
        // Generic ESP32 - try standard SPI pins
        sdCSPin = 5;
        sdMOSIPin = 23;
        sdMISOPin = 19;
        sdSCKPin = 18;
        Serial.println("SD: Using generic ESP32 pins");
    #endif
}

bool StorageManager::detectSDCard() {
    autoDetectSDPins();
    
    if (sdCSPin < 0) {
        Serial.println("SD: No valid pins configured");
        return false;
    }
    
    Serial.printf("SD: Trying CS=%d, MOSI=%d, MISO=%d, SCK=%d\n", 
                  sdCSPin, sdMOSIPin, sdMISOPin, sdSCKPin);
    
    // Try to initialize SD card with SPI
    SPIClass spi = SPIClass(FSPI);
    spi.begin(sdSCKPin, sdMISOPin, sdMOSIPin, sdCSPin);
    
    if (!SD.begin(sdCSPin, spi)) {
        Serial.println("SD: Not detected or failed to initialize");
        return false;
    }
    
    uint8_t cardType = SD.cardType();
    if (cardType == CARD_NONE) {
        Serial.println("SD: No card attached");
        SD.end();
        return false;
    }
    
    Serial.print("SD: Card detected - Type: ");
    switch (cardType) {
        case CARD_MMC:  Serial.println("MMC"); break;
        case CARD_SD:   Serial.println("SDSC"); break;
        case CARD_SDHC: Serial.println("SDHC"); break;
        default:        Serial.println("UNKNOWN"); break;
    }
    
    uint64_t cardSize = SD.cardSize() / (1024 * 1024);
    Serial.printf("SD: Card Size: %llu MB\n", cardSize);
    
    return true;
}

bool StorageManager::checkSDFormatted() {
    if (!sdAvailable) {
        return false;
    }
    
    // Try to open root directory
    File root = SD.open("/");
    if (!root) {
        Serial.println("SD: Cannot open root - not formatted or corrupt");
        return false;
    }
    
    if (!root.isDirectory()) {
        Serial.println("SD: Root is not a directory");
        root.close();
        return false;
    }
    
    root.close();
    
    // Try to create a test file
    File test = SD.open("/test.tmp", FILE_WRITE);
    if (!test) {
        Serial.println("SD: Cannot write test file - may not be formatted");
        return false;
    }
    test.println("test");
    test.close();
    
    // Clean up
    SD.remove("/test.tmp");
    
    Serial.println("SD: Card is properly formatted");
    return true;
}

bool StorageManager::begin() {
    Serial.println("Storage: Initializing...");
    
    // Always initialize LittleFS as fallback
    if (LittleFS.begin(true)) {
        littleFSAvailable = true;
        currentStorage = STORAGE_LITTLEFS;
        Serial.println("Storage: LittleFS initialized");
    } else {
        Serial.println("Storage: LittleFS failed to initialize!");
        return false;
    }
    
    // Load user preference
    storagePrefs.begin("storage", true);  // Read-only
    int savedPref = storagePrefs.getInt("type", STORAGE_LITTLEFS);
    storagePrefs.end();
    
    // Check for SD card
    sdAvailable = detectSDCard();
    
    if (sdAvailable) {
        sdFormatted = checkSDFormatted();
        
        if (sdFormatted && savedPref == STORAGE_SD) {
            // User wants SD and it's available
            currentStorage = STORAGE_SD;
            Serial.println("Storage: Using SD card");
            
            // Check if we need to migrate data from LittleFS
            if (!SD.exists("/friends.ndjson") && LittleFS.exists("/friends.ndjson")) {
                Serial.println("Storage: Migrating data from LittleFS to SD...");
                migrateDataToSD();
            }
        } else if (sdFormatted) {
            Serial.println("Storage: SD card available but LittleFS preferred");
        } else {
            Serial.println("Storage: SD card detected but not formatted");
        }
    } else {
        Serial.println("Storage: No SD card detected - using LittleFS");
    }
    
    Serial.printf("Storage: Active storage is %s\n", getStorageTypeName());
    return true;
}

bool StorageManager::formatSD() {
    if (!sdAvailable) {
        Serial.println("SD: No card available to format");
        return false;
    }
    
    Serial.println("SD: Formatting... (this may take a while)");
    
    // Note: SD library doesn't have a format function
    // We'll create a filesystem structure by creating directories
    
    // Try to remove all files
    File root = SD.open("/");
    if (root) {
        File file = root.openNextFile();
        while (file) {
            if (!file.isDirectory()) {
                String path = String("/") + file.name();
                SD.remove(path.c_str());
            }
            file = root.openNextFile();
        }
        root.close();
    }
    
    // Verify we can write
    File test = SD.open("/test.tmp", FILE_WRITE);
    if (!test) {
        Serial.println("SD: Format failed - cannot write");
        return false;
    }
    test.println("formatted");
    test.close();
    SD.remove("/test.tmp");
    
    sdFormatted = true;
    Serial.println("SD: Format complete");
    return true;
}

bool StorageManager::switchToSD() {
    if (!sdAvailable || !sdFormatted) {
        Serial.println("Storage: Cannot switch to SD - not available or not formatted");
        return false;
    }
    
    // Migrate data if needed
    if (!SD.exists("/friends.ndjson") && LittleFS.exists("/friends.ndjson")) {
        Serial.println("Storage: Migrating data to SD...");
        if (!migrateDataToSD()) {
            Serial.println("Storage: Migration failed");
            return false;
        }
    }
    
    currentStorage = STORAGE_SD;
    
    // Save preference
    storagePrefs.begin("storage", false);
    storagePrefs.putInt("type", STORAGE_SD);
    storagePrefs.end();
    
    Serial.println("Storage: Switched to SD card");
    return true;
}

bool StorageManager::switchToLittleFS() {
    if (!littleFSAvailable) {
        Serial.println("Storage: Cannot switch to LittleFS - not available");
        return false;
    }
    
    // Migrate data if needed
    if (currentStorage == STORAGE_SD && SD.exists("/friends.ndjson") && !LittleFS.exists("/friends.ndjson")) {
        Serial.println("Storage: Migrating data to LittleFS...");
        if (!migrateDataToLittleFS()) {
            Serial.println("Storage: Migration failed");
            return false;
        }
    }
    
    currentStorage = STORAGE_LITTLEFS;
    
    // Save preference
    storagePrefs.begin("storage", false);
    storagePrefs.putInt("type", STORAGE_LITTLEFS);
    storagePrefs.end();
    
    Serial.println("Storage: Switched to LittleFS");
    return true;
}

bool StorageManager::migrateDataToSD() {
    const char* files[] = {"/friends.ndjson", "/packets.ndjson"};
    
    for (const char* filename : files) {
        if (!LittleFS.exists(filename)) {
            Serial.printf("Storage: Skipping %s - doesn't exist in LittleFS\n", filename);
            continue;
        }
        
        Serial.printf("Storage: Migrating %s...\n", filename);
        
        File src = LittleFS.open(filename, FILE_READ);
        if (!src) {
            Serial.printf("Storage: Cannot open source %s\n", filename);
            continue;
        }
        
        File dst = SD.open(filename, FILE_WRITE);
        if (!dst) {
            Serial.printf("Storage: Cannot open destination %s\n", filename);
            src.close();
            return false;
        }
        
        // Copy data in chunks
        uint8_t buffer[512];
        size_t totalBytes = 0;
        while (src.available()) {
            size_t bytesRead = src.read(buffer, sizeof(buffer));
            dst.write(buffer, bytesRead);
            totalBytes += bytesRead;
        }
        
        src.close();
        dst.flush();
        dst.close();
        
        Serial.printf("Storage: Migrated %s (%d bytes)\n", filename, totalBytes);
    }
    
    Serial.println("Storage: Migration to SD complete");
    return true;
}

bool StorageManager::migrateDataToLittleFS() {
    const char* files[] = {"/friends.ndjson", "/packets.ndjson"};
    
    for (const char* filename : files) {
        if (!SD.exists(filename)) {
            Serial.printf("Storage: Skipping %s - doesn't exist on SD\n", filename);
            continue;
        }
        
        Serial.printf("Storage: Migrating %s...\n", filename);
        
        File src = SD.open(filename, FILE_READ);
        if (!src) {
            Serial.printf("Storage: Cannot open source %s\n", filename);
            continue;
        }
        
        File dst = LittleFS.open(filename, FILE_WRITE);
        if (!dst) {
            Serial.printf("Storage: Cannot open destination %s\n", filename);
            src.close();
            return false;
        }
        
        // Copy data in chunks
        uint8_t buffer[512];
        size_t totalBytes = 0;
        while (src.available()) {
            size_t bytesRead = src.read(buffer, sizeof(buffer));
            dst.write(buffer, bytesRead);
            totalBytes += bytesRead;
        }
        
        src.close();
        dst.flush();
        dst.close();
        
        Serial.printf("Storage: Migrated %s (%d bytes)\n", filename, totalBytes);
    }
    
    Serial.println("Storage: Migration to LittleFS complete");
    return true;
}

fs::FS& StorageManager::getFS() {
    if (currentStorage == STORAGE_SD && sdAvailable && sdFormatted) {
        return SD;
    }
    return LittleFS;
}

const char* StorageManager::getStorageTypeName() const {
    switch (currentStorage) {
        case STORAGE_SD: return "SD Card";
        case STORAGE_LITTLEFS: return "LittleFS";
        default: return "None";
    }
}

File StorageManager::open(const char* path, const char* mode) {
    return getFS().open(path, mode);
}

bool StorageManager::exists(const char* path) {
    return getFS().exists(path);
}

bool StorageManager::remove(const char* path) {
    return getFS().remove(path);
}

bool StorageManager::rename(const char* pathFrom, const char* pathTo) {
    return getFS().rename(pathFrom, pathTo);
}

uint64_t StorageManager::getSDCardSize() {
    if (!sdAvailable) return 0;
    return SD.cardSize() / (1024 * 1024);  // MB
}

uint64_t StorageManager::getSDCardUsed() {
    if (!sdAvailable) return 0;
    return SD.usedBytes() / (1024 * 1024);  // MB
}

uint64_t StorageManager::getSDCardFree() {
    if (!sdAvailable) return 0;
    return (SD.cardSize() - SD.usedBytes()) / (1024 * 1024);  // MB
}
