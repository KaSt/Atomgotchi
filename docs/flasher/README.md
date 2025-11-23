# Gotchi Web Flasher

A modern, browser-based firmware flasher for Gotchi devices using the Web Serial API.

## 🌐 Live Web Flasher

**[Flash Your Device Here](https://kast.github.io/Atomgotchi/flasher/)**

## ✨ Features

- **No Driver Installation Required** - Works directly in Chrome/Edge browsers
- **Visual Device Selection** - Easy-to-use interface with device images
- **Real-time Progress** - See exactly what's happening during the flash
- **Automatic Updates** - Always uses the latest firmware from GitHub releases
- **Multi-device Support** - Supports 26+ different ESP32 boards

## 🎯 Supported Devices

### M5Stack
- M5Stack Cardputer ⭐ (Recommended)
- M5StickC Plus2
- M5Atom S3 / S3R
- M5Stack Core / Core2 / CoreS3
- M5Stack Fire, Paper, and more

### LilyGo
- T-Display S3
- T-Display
- T-Embed
- T-Watch S3
- T-Dongle S3
- T-QT Pro

### Others
- Heltec WiFi LoRa 32 V3
- ESP32-S3-DevKitC-1
- ESP32-C3-DevKitM-1
- And more...

## 🚀 How to Use

1. **Open the web flasher** in Chrome or Edge browser
2. **Select your device** from the visual grid
3. **Click "Connect & Flash"**
4. **Select your device's serial port** in the browser popup
5. **Wait for the flash to complete** (usually 1-2 minutes)
6. **Done!** Your device will automatically reboot with Gotchi firmware

## ⚙️ Browser Requirements

- **Supported:** Chrome 89+, Edge 89+, Opera 75+
- **Not Supported:** Firefox, Safari (Web Serial API not available)

## 🔧 Technical Details

### Firmware Sources

Firmware files are automatically built and published by GitHub Actions:
- **Workflow:** `.github/workflows/build-release.yml`
- **Trigger:** Every push to main/master branch or manual trigger
- **Artifacts:** Stored in GitHub Releases
- **Web Flasher:** Automatically updated with latest firmware

### File Structure

```
docs/flasher/
├── index.html          # Main web flasher page
├── firmware/           # Firmware binaries (auto-updated)
│   ├── index.json      # Firmware metadata
│   ├── gotchi-*.bin    # Device-specific firmwares
│   └── manifest-*.json # Flash manifests
└── images/             # Device images (optional)
```

### Flash Process

1. **Connection:** Establishes serial connection at 115200 baud
2. **Chip Detection:** Auto-detects ESP32 variant (ESP32/S3/C3)
3. **Erase:** Erases existing flash memory
4. **Write:** Writes firmware binary to address 0x0
5. **Verify:** Verifies written data
6. **Reset:** Hardware reset to boot new firmware

## 🛠️ Development

### Local Testing

```bash
# Serve locally
cd docs/flasher
python -m http.server 8000

# Open browser
open http://localhost:8000
```

### Adding Device Images

Place device images in `docs/flasher/images/` and update the device database in `index.html`:

```javascript
{
    id: "m5stack-cardputer",
    name: "M5Stack Cardputer",
    image: "images/cardputer.jpg"  // Add this line
}
```

### Firmware Build Process

See `.github/workflows/build-release.yml` for the complete build pipeline:

1. **Prepare Release:** Read VERSION file, generate changelog
2. **Build Firmware:** Compile for all devices using PlatformIO
3. **Create Release:** Tag and create GitHub release
4. **Update Flasher:** Copy firmware to gh-pages branch

## 📝 Version Management

Version is controlled by the `VERSION` file in the repository root:

```bash
# Update version
echo "1.1.0" > VERSION
git add VERSION
git commit -m "Bump version to 1.1.0"
git push

# Automatic actions:
# 1. GitHub Actions builds all firmware
# 2. Creates git tag v1.1.0
# 3. Creates GitHub release with binaries
# 4. Updates web flasher firmware
```

## 🔐 Security Notes

- All firmware is built from source in GitHub Actions
- SHA-256 checksums available in release notes
- No third-party binary hosting
- Open-source and auditable build process

## 🐛 Troubleshooting

**"Serial port access denied"**
- Ensure no other program is using the serial port
- Try a different USB cable
- On Linux, add your user to `dialout` group

**"Failed to connect"**
- Hold BOOT button while connecting (if device has one)
- Try slower baud rate in browser console
- Check USB cable supports data transfer

**"Firmware download failed"**
- Check internet connection
- Verify device is in supported list
- Clear browser cache and try again

## 📄 License

Same as main Gotchi project. See repository root for details.

## 🙏 Credits

- **Web Serial API:** Chrome/Edge teams
- **ESPTool-JS:** Espressif Systems
- **Inspiration:** Meshtastic and Bruce firmware web flashers
