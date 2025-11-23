# Quick Reference - Release Management

## 🚀 Common Tasks

### Release New Version

```bash
# 1. Update VERSION file
echo "1.1.0" > VERSION

# 2. Commit and push
git add VERSION
git commit -m "Release v1.1.0"
git push origin main

# ✅ Automatic: Builds, tags, creates release, updates web flasher
```

### Manual Trigger Build

1. GitHub → Actions → "Build and Release Firmware"
2. Click "Run workflow"
3. (Optional) Override version
4. Click "Run workflow"

### Check Build Status

```bash
# View latest commit status
git log --oneline -1

# Or check on GitHub
# ✅ = Build succeeded
# ❌ = Build failed
```

### Download Firmware

**For Users:**
- GitHub → Releases → Latest → Download .bin file
- Or use: https://kast.github.io/Atomgotchi/flasher/

**For Development:**
- GitHub → Actions → Workflow run → Artifacts

## 🔧 Troubleshooting

### Build Fails

```bash
# Test locally
pio run -e m5stack-cardputer

# Check specific board
pio run -e lilygo-t-display-s3 -v
```

### Re-run Failed Build

1. GitHub → Actions → Failed workflow
2. Click "Re-run failed jobs"

### Delete and Re-create Release

```bash
# Delete tag
git tag -d v1.0.0
git push origin :refs/tags/v1.0.0

# Delete release on GitHub UI
# Then push again to recreate
git push origin main
```

## 📋 File Locations

```
.
├── VERSION ..................... Version number (update this)
├── platformio.ini .............. Board configs (26 boards)
├── BUILD_SYSTEM.md ............. Full documentation
├── .github/workflows/
│   ├── build-release.yml ....... Build & release automation
│   └── deploy-pages.yml ........ GitHub Pages deployment
└── docs/flasher/
    ├── index.html .............. Web flasher UI
    ├── README.md ............... Flasher docs
    └── firmware/ ............... Auto-updated firmwares
```

## 🌐 URLs

- **Web Flasher:** https://kast.github.io/Atomgotchi/flasher/
- **Repository:** https://github.com/KaSt/Atomgotchi
- **Releases:** https://github.com/KaSt/Atomgotchi/releases
- **Actions:** https://github.com/KaSt/Atomgotchi/actions

## 🎯 Supported Devices (Primary 11)

1. M5Stack Cardputer (ESP32-S3) ⭐
2. M5StickC Plus2 (ESP32-PICO)
3. M5Atom S3 (ESP32-S3)
4. M5Atom S3R (ESP32-S3)
5. M5Stack Core (ESP32)
6. M5Stack Core2 (ESP32)
7. M5Stack CoreS3 (ESP32-S3)
8. LilyGo T-Display S3 (ESP32-S3)
9. LilyGo T-Display (ESP32)
10. LilyGo T-Embed (ESP32-S3)
11. Heltec WiFi LoRa 32 V3 (ESP32-S3)

Plus 15+ more in platformio.ini!

## 📝 Release Checklist

- [ ] Update VERSION file
- [ ] Test build locally: `pio run -e m5stack-cardputer`
- [ ] Commit with descriptive message
- [ ] Push to main branch
- [ ] Wait for Actions to complete (~15 min)
- [ ] Verify release created on GitHub
- [ ] Test web flasher with new firmware
- [ ] Update documentation if needed

## 🔐 Permissions Required

**GitHub Pages:**
- Settings → Pages → Source: "GitHub Actions"

**GitHub Token:**
- Automatically provided (no setup needed)

**Workflow Permissions:**
- Settings → Actions → General → Workflow permissions
- Enable "Read and write permissions"
- Enable "Allow GitHub Actions to create and approve pull requests"
