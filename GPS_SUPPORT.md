# GPS Module Support

## Overview

Palnagotchi supports GPS modules for geolocation tracking of discovered friends. GPS coordinates are automatically saved when a valid GPS fix is available.

## Supported GPS Modules

### M5StickC / StickC Plus / StickC Plus 2
- **M5Stack GPS Module (1.1)** - Grove connector
  - Pins: G32 (RX), G33 (TX)
  - Baud: 9600
  - Auto-detected on boot

- **GPS Hat for StickC**
  - Pins: G0 (RX), G26 (TX)
  - Baud: 9600
  - Fallback detection if Grove not found

### M5Atom S3
- **Atomic GPS Base**
  - Pins: G2 (RX), G1 (TX)
  - Baud: 9600
  - Includes SD card slot

### M5Stack Cardputer
- **Grove GPS Module (Port A)**
  - Pins: G1 (RX), G2 (TX)
  - Baud: 9600
  - Connected via Grove Port A

### Generic ESP32
- Fallback pins: G16 (RX), G17 (TX)

## Features

### Hot-Plug Detection
- GPS modules can be connected at boot or later
- System checks for GPS data on startup
- 3-second detection window
- No errors if GPS not present

### Valid Fix Detection
- Checks for NMEA data stream
- Validates GPS fix status (State == 'A')
- Requires satellite lock before saving coordinates

### Automatic Coordinate Saving
- Coordinates saved when friend is discovered
- Only saves when valid GPS fix available
- Includes hemisphere corrections (N/S, E/W)
- Precision: 6 decimal places (~0.1m accuracy)

## Data Storage

### Friend Database Format
GPS coordinates are stored in `/friends.ndjson`:

```json
{
  "name": "alice",
  "identity": "abc123...",
  "timestamp": 1234567890,
  "latitude": 51.507351,
  "longitude": -0.127758,
  "has_gps": true,
  "rssi": -45,
  "channel": 6
}
```

### Fields
- `latitude`: Decimal degrees, negative for South
- `longitude`: Decimal degrees, negative for West
- `has_gps`: Boolean flag indicating GPS data presence

## API Functions

### Check GPS Status

```cpp
#include "pwn.h"

// Check if GPS module is present
if (hasGPSModule()) {
    Serial.println("GPS module detected");
}

// Check if GPS is actively receiving data
if (isGPSConnected()) {
    Serial.println("GPS connected and receiving data");
}

// Check if GPS has valid satellite fix
if (hasGPSFix()) {
    Serial.println("GPS has valid fix");
    
    double lat, lon;
    getGPSCoordinates(lat, lon);
    Serial.printf("Location: %.6f, %.6f\n", lat, lon);
}
```

### Manual Coordinate Retrieval

```cpp
double latitude, longitude;
getGPSCoordinates(latitude, longitude);

if (latitude != 0.0 || longitude != 0.0) {
    Serial.printf("📍 Lat: %.6f, Lon: %.6f\n", latitude, longitude);
}
```

## Serial Output Examples

### Successful GPS Initialization
```
GPS: Attempting to initialize on RX=32, TX=33, baud=9600
GPS: Data detected!
GPS: Initialized successfully
GPS: Task started
GPS: Task running
```

### No GPS Module
```
GPS: Attempting to initialize on RX=32, TX=33, baud=9600
GPS: No data received - GPS may not be connected
GPS: Trying StickC Hat port (G0/G26)
GPS: No data received - GPS may not be connected
GPS: No GPS module detected - continuing without GPS
```

### Friend Discovery with GPS
```
📍 Spotted alice at: Lat=51.507351, Lon=-0.127758 (Fix: A)
Friend added to DB (with GPS)
```

### Friend Discovery without GPS Fix
```
⚠️  GPS has no fix - waiting for satellites...
Friend added to DB
```

## NMEA Sentences Parsed

- **GNRMC** - Recommended Minimum Specific GPS/Transit Data
  - Position, velocity, time
  - Fix status (A=Active, V=Void)
  - Primary source for coordinates

- **GPGSV** - GPS Satellites in View
  - Satellite count and signal strength
  - Used for debugging

- **GNGSA** - GPS DOP and Active Satellites
  - Dilution of Precision
  - Not currently used

## Troubleshooting

### GPS Not Detected

1. **Check connections**
   - Verify Grove cable is fully inserted
   - Check if GPS LED is blinking (indicates power)

2. **Verify baud rate**
   - Most M5Stack GPS modules use 9600 baud
   - Some modules may use 38400 or 115200

3. **Enable debug output**
   - Uncomment `#define DEBUG_GPS` in `GPSAnalyse.h`
   - Rebuild and monitor serial output

4. **Check pin configuration**
   - Ensure correct RX/TX pins for your device
   - Grove connectors may be labeled differently

### No GPS Fix

- **Wait for satellite lock** (can take 30-120 seconds outdoors)
- **Ensure clear sky view** (GPS doesn't work indoors)
- **Check antenna** (some modules have external antenna)
- **Verify NMEA output** (should show `$GNRMC...A...` when locked)

### Coordinates Not Saving

1. Check serial output for GPS status messages
2. Verify `has_gps` field in database
3. Ensure `State == 'A'` in NMEA data
4. Check LittleFS has space available

## Performance

- **Detection time**: ~3 seconds on boot
- **Fix time**: 30-120 seconds (cold start)
- **Fix time**: 1-30 seconds (warm start)
- **Update rate**: 1 Hz (1 update/second)
- **Memory overhead**: ~4KB per GPS task
- **Power consumption**: ~30-50mA (GPS module dependent)

## Future Enhancements

- [ ] GPS track logging (GPX export)
- [ ] Speed and altitude tracking
- [ ] Distance calculations between friends
- [ ] Geofencing alerts
- [ ] Multiple coordinate systems (UTM, MGRS)
- [ ] GPS time synchronization
- [ ] A-GPS for faster fix
- [ ] Battery optimization (periodic GPS)

## References

- [NMEA 0183 Protocol](https://www.nmea.org/)
- [M5Stack GPS Module Documentation](https://docs.m5stack.com/en/unit/gps)
- [Arduino HardwareSerial](https://www.arduino.cc/reference/en/language/functions/communication/serial/)
