# SD Card Storage Support

## Overview

Palnagotchi supports external SD card storage for saving friend and packet data. The system automatically detects SD cards, handles formatting, and can migrate data between internal flash (LittleFS) and SD card storage.

## Supported Hardware

### M5Atom S3 with Atomic GPS Base
- **Built-in SD card slot**
- Pins: CS=5, MOSI=23, MISO=19, SCK=18
- Supports microSD cards (FAT32)
- Best option for large-scale data collection

### M5StickC Plus / Plus 2
- **SD card hat/accessory** (optional)
- Pins: CS=4, MOSI=23, MISO=19, SCK=18
- Requires additional hardware

### M5Stack Cardputer
- **SD slot on some versions**
- Pins: CS=12, MOSI=13, MISO=11, SCK=14
- Check your specific model

### Generic ESP32
- **External SD module** via SPI
- Default pins: CS=5, MOSI=23, MISO=19, SCK=18
- Any microSD card module compatible with ESP32

## Features

### Automatic Detection
- SD card presence detected on boot
- No errors if SD card not present
- Hot-swap detection (reboot required)

### Format Check
- Automatically verifies SD card is formatted
- Prompts user if card needs formatting
- Supports FAT32 filesystem

### Smart Data Migration
- Automatically migrates data from internal to SD when switching
- Preserves all friend and packet data
- Bidirectional: Internal ↔ SD

### User Preference Saving
- Remembers storage choice across reboots
- Stored in NVS (non-volatile storage)
- Can be changed anytime via menu

## Menu System

### Accessing Storage Settings
```
Main Menu
└── Settings
    └── Storage
```

### Storage Menu Options

**When SD Card is Available and Formatted:**
- `Current: SD Card` or `Current: LittleFS` - Shows active storage
- `Switch to SD` - Migrate and use SD card
- `Switch to Internal` - Migrate and use internal flash
- `SD Card Info` - View size, used, free space
- `< Back` - Return to settings

**When SD Card is Not Formatted:**
- `Current: LittleFS` - Shows active storage
- `Format SD Card` - Initialize SD card (with confirmation)
- `< Back` - Return to settings

**When No SD Card:**
- `Current: LittleFS` - Shows active storage
- `No SD Card` - Disabled option
- `< Back` - Return to settings

## Usage Guide

### First Time Setup with SD Card

1. **Insert SD Card** into device (power off recommended)
2. **Power on** the device
3. Check serial output for detection:
   ```
   SD: Detected Atom S3 - using Atomic GPS pins
   SD: Card detected - Type: SDHC
   SD: Card Size: 16384 MB
   ```

4. If card is not formatted, you'll see:
   ```
   SD: Cannot open root - not formatted or corrupt
   SD: Card detected but not formatted
   Storage: Using LittleFS for storage
   ```

5. **Navigate to Menu:**
   - Long press button → Settings → Storage
   - Select "Format SD Card"
   - Confirmation prompt appears

6. **Format SD Card:**
   - Long press to confirm format
   - Wait for "Success!" message

7. **Switch to SD Storage:**
   - Select "Switch to SD"
   - Data automatically migrates
   - Confirmation: "Using SD Card"

### Switching Between Storage Types

**Switch to SD Card:**
1. Menu → Settings → Storage
2. Select "Switch to SD"
3. Data migrates automatically
4. "Using SD Card" confirmation

**Switch to Internal:**
1. Menu → Settings → Storage
2. Select "Switch to Internal"
3. Data migrates back
4. "Using Internal" confirmation

### View Storage Information

1. Menu → Settings → Storage → SD Card Info
2. See details:
   ```
   Active: SD Card
   
   SD Card:
     Size: 16384 MB
     Used: 2 MB
     Free: 16382 MB
     Status: OK
   ```

## Data Migration

### Automatic Migration Process

When switching storage, the system:

1. **Checks source data** - Verifies files exist
2. **Creates destination files** - Prepares target storage
3. **Copies in chunks** - 512-byte buffer for efficiency
4. **Verifies integrity** - Ensures complete copy
5. **Updates preference** - Saves storage choice

### Migrated Files
- `/friends.ndjson` - Friend database
- `/packets.ndjson` - Captured packets

### Migration Console Output
```
Storage: Migrating data to SD...
Storage: Migrating /friends.ndjson...
Storage: Migrated /friends.ndjson (4523 bytes)
Storage: Migrating /packets.ndjson...
Storage: Migrated /packets.ndjson (15234 bytes)
Storage: Migration to SD complete
Storage: Switched to SD card
```

## Technical Details

### Storage Manager API

```cpp
#include "storage.h"

// Check SD availability
if (storage.isSDAvailable()) {
    Serial.println("SD card present");
}

// Check if SD is formatted
if (storage.isSDFormatted()) {
    Serial.println("SD card ready");
}

// Get active storage type
StorageType type = storage.getStorageType();
// Returns: STORAGE_SD or STORAGE_LITTLEFS

// Get active filesystem
fs::FS& fs = storage.getFS();

// File operations (use active storage automatically)
File f = storage.open("/data.txt", FILE_WRITE);
bool exists = storage.exists("/data.txt");
storage.remove("/data.txt");
storage.rename("/old.txt", "/new.txt");

// Get SD card info
uint64_t size = storage.getSDCardSize();   // MB
uint64_t used = storage.getSDCardUsed();   // MB
uint64_t free = storage.getSDCardFree();   // MB
```

### Database Integration

The database layer automatically uses the storage manager:

```cpp
// db.cpp automatically uses active storage
File f = storage.open(FR_TBL, FILE_WRITE);  // Uses SD or LittleFS
```

### Storage Preference

Stored in NVS namespace "storage":
- Key: `type`
- Value: `1` (LittleFS) or `2` (SD Card)

## Troubleshooting

### SD Card Not Detected

**Symptoms:**
```
SD: No card attached
SD: Not detected or failed to initialize
```

**Solutions:**
1. Check SD card is fully inserted
2. Verify SD card is not write-protected
3. Try a different SD card (some cards incompatible)
4. Check SD card size (max 32GB for FAT32)
5. Verify pins match your hardware

### SD Card Won't Format

**Symptoms:**
```
SD: Format failed - cannot write
```

**Solutions:**
1. Format card on computer first (FAT32)
2. Check for physical damage to card
3. Try different SD card
4. Verify card is not fake/counterfeit

### Data Migration Failed

**Symptoms:**
```
Storage: Migration failed
```

**Solutions:**
1. Check SD card has enough free space
2. Verify SD card is not full
3. Check both storage systems are working
4. Try manual file copy if needed

### Wrong Storage Active

**Symptoms:**
- Expected SD but using internal
- Expected internal but using SD

**Solutions:**
1. Check storage preference in NVS
2. Navigate to Storage menu
3. Manually switch to desired storage
4. Verify with "SD Card Info"

## Best Practices

### When to Use SD Card
- ✅ Large-scale data collection
- ✅ Long-term deployment
- ✅ Need removable storage
- ✅ Want to analyze data on PC

### When to Use Internal Storage
- ✅ No SD card available
- ✅ Want faster access
- ✅ Prefer simplicity
- ✅ Limited deployment duration

### SD Card Recommendations
- **Size:** 4GB - 32GB (FAT32 limit)
- **Speed:** Class 10 or higher
- **Brand:** SanDisk, Samsung, Kingston
- **Type:** MicroSD with adapter if needed

### Data Backup
- Periodically copy files from device
- Use SD card for easy PC access
- Keep backup of important captures

## Performance

| Operation | LittleFS | SD Card |
|-----------|----------|---------|
| Read Speed | Fast | Medium |
| Write Speed | Fast | Medium |
| Capacity | ~3MB | Up to 32GB |
| Durability | High | Medium |
| Removability | No | Yes |
| Power Usage | Low | Medium |

## Console Output Examples

### Successful SD Detection
```
SD: Detected Atom S3 - using Atomic GPS pins
SD: Trying CS=5, MOSI=23, MISO=19, SCK=18
SD: Card detected - Type: SDHC
SD: Card Size: 16384 MB
SD: Card is properly formatted
Storage: Using SD card for storage
DB: Using SD Card for storage
```

### SD Formatting
```
SD: Formatting... (this may take a while)
SD: Format complete
Storage: Switched to SD card
```

### Data Migration
```
Storage: Migrating data to SD...
Storage: Migrating /friends.ndjson...
Storage: Migrated /friends.ndjson (4523 bytes)
Storage: Migration to SD complete
```

## Future Enhancements

- [ ] SD card hot-swap detection
- [ ] Automatic backup scheduling
- [ ] GPX export directly to SD
- [ ] Log file rotation
- [ ] Multi-file export formats
- [ ] SD card health monitoring
- [ ] Wear leveling optimization
- [ ] Encrypted SD storage

## References

- [ESP32 SD Library](https://github.com/espressif/arduino-esp32/tree/master/libraries/SD)
- [LittleFS Documentation](https://github.com/littlefs-project/littlefs)
- [FAT32 Specification](https://en.wikipedia.org/wiki/File_Allocation_Table)
