# Gotchi - Educational WiFi Security Research Platform

![Gotchi on M5Stack Cardputer](palnagotchi.jpg)

## ⚠️ Disclaimer

**This project is intended for personal education and private network security research only.** It is designed to help users understand WiFi security mechanisms, network protocols, and embedded systems programming. Only use this software on networks you own or have explicit permission to test.

## 📖 Overview

Gotchi is an educational WiFi security research platform that runs on M5Stack devices. The project evolved from a fork of [Palnagotchi](https://github.com/viniciusbo/m5-palnagotchi), initially created to provide companionship to Pwnagotchis through the Pwngrid friend protocol. Over time, it has grown to replicate several Pwnagotchi features and now includes its own lightweight "AI" decision-making system.

### Project Evolution

1. **Phase 1: Palnagotchi Fork** - Started as a friendly companion that could be detected by Pwnagotchis via Pwngrid beacons
2. **Phase 2: Feature Replication** - Implemented WPA handshake capture, PMKID detection, and packet analysis
3. **Phase 3: AI Implementation** - Added a Q-Learning based decision engine for autonomous channel hopping and network analysis

## 🧠 The "AI" System

The term "AI" might be grandiose for what is essentially a **Q-Learning reinforcement learning agent**, but it's far more appropriate than running full ML models on resource-constrained devices like the Raspberry Pi Zero. Our implementation is specifically designed for embedded systems with limited RAM and CPU.

### How It Works

The AI uses **Q-Learning** (a reinforcement learning algorithm) to make decisions about:
- Which WiFi channel to monitor
- When to perform deauthentication attacks
- How long to observe each channel
- When to stay idle to save power

**State Space (312 possible states):**
- **Channel** (13 options): WiFi channels 1-13
- **AP Density** (3 levels): Empty, Low (1-2 APs), High (3+ APs)
- **Recent Success** (2 states): Captured handshake/PMKID recently or not
- **Time of Day** (4 buckets): Night, Morning, Afternoon, Evening

**Action Space (5 actions):**
- `STAY`: Continue monitoring current channel
- `NEXT`: Move to next channel
- `PREV`: Move to previous channel
- `DEAUTH`: Send deauthentication frames to trigger handshake
- `IDLE`: Enter low-power observation mode

**Learning Process:**
The agent receives rewards for:
- ✅ **+10** for capturing WPA handshakes
- ✅ **+8** for detecting PMKIDs
- ✅ **+1** for discovering new access points
- ❌ **-0.1** per timestep (encourages efficiency)

Over time (epochs), the agent learns which actions work best in different scenarios. The Q-table (knowledge matrix) is persisted to NVS flash memory, so learning continues across reboots.

**Hyperparameters:**
- Learning rate (α): 0.15
- Discount factor (γ): 0.90
- Exploration rate (ε): 0.40 → 0.05 (decays over time)
- Epoch tracking: Stored in EEPROM

### Why Not "Real" AI?

Traditional deep learning models require:
- Large memory footprint (100s of MB)
- Floating-point GPU acceleration
- Continuous training data pipelines
- High power consumption

Q-Learning provides:
- Tiny memory footprint (~15KB for Q-table)
- Runs efficiently on ESP32 @ 240MHz
- Simple state-action mapping
- Battery-friendly operation

## ✨ Features

### Core Functionality

#### 🌐 Pwngrid Protocol Implementation
- **Beacon Advertisement**: Broadcasts presence to nearby Pwnagotchis using custom WiFi beacon frames
- **Friend Detection**: Discovers and tracks other Pwnagotchis and Gotchi units
- **Identity Exchange**: JSON-serialized payloads in WiFi vendor-specific IE frames
- **Policy Sanitization**: Brain policy parameters removed to avoid interfering with real Pwnagotchi learning

#### 📡 WiFi Security Research
- **WPA Handshake Capture**: Detects and logs 4-way EAPOL handshakes
- **PMKID Extraction**: Captures PMKIDs from beacon/probe responses
- **Packet Analysis**: Real-time 802.11 frame parsing and classification
- **Channel Hopping**: Smart or AI-driven channel selection
- **Deauthentication**: Controlled deauth frames for handshake capture (AI mode only)

#### 🎭 Personality Modes

**Friendly Mode:**
- Passive monitoring only
- No deauthentication attacks
- Simple random channel hopping
- Safe for general use and observation

**AI Mode:**
- Active Q-Learning agent
- Autonomous decision making
- Optimizes for handshake/PMKID capture
- Deauthentication when strategically beneficial
- Learns from experience across reboots

#### 📍 GPS Integration
- Automatic GPS module detection
- Geolocation tagging for discovered friends
- Multiple GPS module support (see [GPS_SUPPORT.md](GPS_SUPPORT.md))
- NMEA sentence parsing (GNRMC, GPGSV, GNGSA)
- Coordinates stored in friend database with 6-decimal precision

#### 💾 Storage Options
- **Internal (LittleFS)**: ~3MB flash storage
- **External (SD Card)**: Up to 32GB microSD support
- Automatic data migration between storage types
- Friend database in NDJSON format
- Packet capture logging
- See [SD_CARD_SUPPORT.md](SD_CARD_SUPPORT.md) for details

#### 🎨 Modern Menu System
- Screen-size adaptive layout
- Scrollbar for long lists
- Keyboard/button navigation
- Inspired by Bruce Predatory Firmware
- See [MENU_SYSTEM.md](MENU_SYSTEM.md) and [MENU_VISUAL_GUIDE.md](MENU_VISUAL_GUIDE.md)

#### ⚙️ WiFi Configuration Portal
- Access Point mode for easy setup
- Web-based configuration interface
- Settings: Device name, brightness, personality mode
- SSID: `Palnagotchi-Config` / Password: `palnagotchi`
- Access at `http://192.168.4.1`
- Auto-timeout after 5 minutes

### Visual Feedback

- **Mood System**: Randomized ASCII faces that change every ~60 seconds
- **Status Bar**: Battery level, uptime, personality mode, channel
- **Friend Counter**: Current session and all-time totals
- **RSSI Indicator**: Signal strength bars for nearby friends
- **Handshake Display**: Shows captured WPA handshakes (AI mode)

## 🎮 Supported Devices

### ✅ Tested & Working
- **M5Stack Cardputer** - Full keyboard support, recommended device
- **M5StickC Plus 2** - Compact form factor with buttons
- **M5Atom S3 / S3R** - Tiny display, touch navigation

### 🔧 Potentially Compatible (Untested)
- M5StickC / M5StickC Plus
- M5Stack Core / Core2
- Other ESP32-based M5Stack devices

## 🎯 Usage

### Button Controls

**M5Stack Cardputer:**
- `ESC` or `m`: Toggle menu
- Arrow keys / `Tab`: Navigate menu
- `Enter` / `.` / `/`: Select option
- `ESC` / `m`: Back to main menu

**M5StickC Plus 2:**
- **Long press** M5 button: Toggle menu
- Top/bottom buttons: Navigate menu
- **Short press** M5 button: Select option

**M5Atom S3 / S3R:**
- **Long press** display: Toggle menu
- Double/triple tap: Navigate menu
- **Short press**: Select option

### Menu Structure

```
Main Menu
├── Friends ──────── View nearby Pwnagotchis/Gotchis with RSSI
├── Settings
│   ├── Personality ─── Switch between Friendly/AI modes
│   ├── Ninja Mode ──── Toggle display on/off for stealth
│   ├── WiFi Config ─── Access Point configuration portal
│   ├── Storage ─────── Switch between SD/Internal, view info
│   └── < Back
└── About ──────────── Device info, version, stats
```

## 🚀 Installation

### Option 1: Web Flasher (Recommended ⭐)

The easiest way to install Gotchi is using our web-based flasher:

**[👉 Flash Your Device Now](https://kast.github.io/Atomgotchi/flasher/)**

1. Open the link above in **Chrome or Edge** browser
2. Select your device from the visual grid
3. Click "Connect & Flash" and choose your device's serial port
4. Wait 1-2 minutes for the flash to complete
5. Done! Your device will reboot with Gotchi firmware

**Requirements:**
- Chrome 89+, Edge 89+, or Opera 75+ (Web Serial API support)
- USB cable connected to your device
- No drivers or software installation needed!

### Option 2: PlatformIO (For Developers)

```bash
# Clone repository
git clone https://github.com/KaSt/Atomgotchi.git
cd Atomgotchi

# Install PlatformIO
pip install platformio

# Build and flash (example for Cardputer)
pio run -e m5stack-cardputer -t upload

# Build for all devices
pio run
```

### Option 3: Manual Flash with ESPTool

```bash
# Download latest firmware from releases
# https://github.com/KaSt/Atomgotchi/releases/latest

# Install esptool
pip install esptool

# Flash to device (example for ESP32-S3)
esptool.py --chip esp32s3 --port /dev/ttyUSB0 write_flash 0x0 gotchi-*.bin
```

## ⚙️ First-Time Setup

1. **Power on** - Device will use "Gotchi" as default name
2. **(Optional) Configure**:
   - Open menu → Settings → WiFi Config (AP)
   - Connect phone to `Palnagotchi-Config` (password: `palnagotchi`)
   - Browse to `http://192.168.4.1`
   - Set device name, brightness, personality
   - Save configuration
3. **(Optional) Choose personality**:
   - Menu → Settings → Personality → AI or Friendly
4. **Start exploring!** The device will immediately begin advertising and detecting friends

### Personality Mode Selection

**Choose Friendly if you want to:**
- Passively observe WiFi networks
- Make friends with Pwnagotchis safely
- Learn about Pwngrid protocol
- Test GPS/SD card functionality

**Choose AI if you want to:**
- Experience reinforcement learning in action
- Capture WPA handshakes for research
- Let the agent optimize channel selection
- Test advanced packet capture features

## 📊 Display Information

### Top Bar
```
CH 6 [A]    BT -  UPS 98%  UP 00:43:12
```
- `CH 6`: Current WiFi channel
- `[A]` or `[F]`: Personality (AI or Friendly)
- `BT`: Bluetooth status
- `UPS`: Battery percentage
- `UP`: Uptime

### Main Display
```
       (^_^)
  
  Feeling happy today!
```
- ASCII art face (mood)
- Status message

### Bottom Bar
```
(-__-)||||  Palnagotchi 0 (0)      PWND 0 (175)
```
- Friend signal strength indicator
- Current session friends / All-time friends (in parentheses)
- Handshakes captured in session / All-time (AI mode only)

## 🔬 Technical Details

### Pwngrid Protocol

Gotchi implements the Pwngrid advertisement protocol by:
1. Crafting custom WiFi beacon frames
2. Embedding JSON payload in vendor-specific information elements (IEs)
3. Broadcasting on channel 1 periodically
4. Listening for beacons from other devices
5. Parsing JSON to extract friend information

**Beacon Payload Example:**
```json
{
  "name": "YourGotchi",
  "identity": "abc123...",
  "pwnd_run": 0,
  "pwnd_tot": 175,
  "uptime": 2592,
  "epoch": 1,
  "face": "(^_^)",
  "policy": {
    "advertise": true,
    "bond_encounters_factor": 20000,
    "bored_num_epochs": 0,
    "channels": [1,2,3,4,5,6,7,8,9,10,11,12,13],
    "excited_num_epochs": 9999,
    "min_recon_time": 5,
    "min_rssi": -200,
    "personality": "Friendly",
    "sad_num_epochs": 0
  }
}
```

### Q-Learning Implementation

The AI maintains a Q-table: `Q[state][action] → expected reward`

**Update Rule:**
```
Q(s,a) ← Q(s,a) + α[r + γ·max(Q(s',a')) - Q(s,a)]
```

Where:
- `s`: Current state
- `a`: Action taken
- `r`: Reward received
- `s'`: Next state
- `α`: Learning rate
- `γ`: Discount factor

**Exploration vs Exploitation:**
- ε-greedy strategy: Random action with probability ε, otherwise best action
- ε decays from 0.40 to 0.05 over ~1000 epochs
- Prevents getting stuck in local optima

### Data Persistence

**EEPROM Storage:**
- Friend totals (8 bytes)
- Handshake totals (8 bytes)
- AI epoch counter (4 bytes)
- Configuration magic byte (1 byte)

**NVS Flash Storage:**
- Q-table matrix (15KB)
- AI hyperparameters
- Storage preferences
- Device configuration

**File Storage (LittleFS/SD):**
- `/friends.ndjson`: Friend database with GPS coordinates
- `/packets.ndjson`: Captured packet metadata

## 🛠️ Building from Source

### Prerequisites
- PlatformIO Core 6.0+ or PlatformIO IDE
- Python 3.7+

### Dependencies (Auto-installed)
```
M5Unified v0.1.16+
M5GFX v0.1.16+
ArduinoJson v7.2.0+
ESP32 Arduino Framework 3.0.7+
```

### Build Commands
```bash
# Build for specific device
pio run -e m5stack-cardputer
pio run -e lilygo-t-display-s3
pio run -e m5stack-atoms3

# Build all 26+ board variants
pio run

# Upload to device
pio run -e m5stack-cardputer -t upload

# Clean build
pio run -t clean
```

### CI/CD Builds

Every push to the main branch automatically:
1. Builds firmware for all supported devices
2. Creates a GitHub release with version tag
3. Uploads firmware binaries to releases
4. Updates the web flasher with latest firmware

See `.github/workflows/build-release.yml` for details.

### Build Outputs
- Program storage: ~1.3MB (varies by board)
- Global variables: ~107KB RAM
- Build time: ~2-3 minutes per board

## 📁 Project Structure

```
Gotchi/
├── palnagotchi/
│   ├── palnagotchi.ino    # Main sketch
│   ├── ai.cpp/h           # Q-Learning agent
│   ├── pwn.cpp/h          # WiFi & Pwngrid protocol
│   ├── ui.cpp/h           # Display & menu system
│   ├── menu_system.cpp/h # Adaptive menu framework
│   ├── config.cpp/h       # Device configuration
│   ├── db.cpp/h           # Friend database
│   ├── storage.cpp/h      # SD card manager
│   ├── ap_config.cpp/h    # Web config portal
│   └── ...
├── GPS_SUPPORT.md         # GPS documentation
├── SD_CARD_SUPPORT.md     # Storage documentation
├── MENU_SYSTEM.md         # Menu framework docs
├── MENU_VISUAL_GUIDE.md   # UI guidelines
└── README.md              # This file
```

## 🔐 Security & Ethics

**This tool is for educational purposes only.**

### Legal Use Cases
- Testing your own WiFi networks
- Security research in controlled lab environments
- Learning about 802.11 protocol internals
- Understanding reinforcement learning concepts

### Illegal Use Cases
- Accessing networks without permission
- Disrupting public WiFi services
- Collecting data from networks you don't own
- Any use that violates local computer crime laws

**The developers are not responsible for misuse of this software.**

## 🤝 Contributing

This project welcomes contributions! Areas of interest:
- Support for additional M5Stack devices
- UI/UX improvements
- AI algorithm enhancements
- Bug fixes and optimization
- Documentation improvements

## 📜 License

This project maintains the original license from the Palnagotchi fork. See individual file headers for details.

## 🙏 Acknowledgments

- **Original Palnagotchi** by [viniciusbo](https://github.com/viniciusbo/m5-palnagotchi)
- **Pwnagotchi** project for the Pwngrid protocol and inspiration
- **M5Stack** for excellent hardware platforms
- **Bruce Predatory Firmware** for menu system inspiration
- The embedded WiFi security research community

## 📚 Additional Resources

- [GPS Module Support](GPS_SUPPORT.md) - Detailed GPS setup and API
- [SD Card Storage](SD_CARD_SUPPORT.md) - Storage management guide
- [Menu System](MENU_SYSTEM.md) - Menu framework documentation
- [Visual Guide](MENU_VISUAL_GUIDE.md) - UI design guidelines

## 🐛 Known Issues

- Some SD cards may have compatibility issues (try different brands)
- GPS cold start can take 30-120 seconds outdoors
- AI mode uses more battery than Friendly mode
- Menu may lag on Atom S3 with many items (small screen)

## 🔮 Future Ideas

- [ ] Bluetooth Low Energy friend detection
- [ ] LoRa long-range friend networking
- [ ] Deep Q-Networks (DQN) for more complex learning
- [ ] Web dashboard for statistics
- [ ] GPX track export for friend locations
- [ ] Multi-language support
- [ ] Custom face/mood designer
- [ ] Plugin system for extensions

---

**Stay curious, stay legal, stay learning! 🎓🔐**
