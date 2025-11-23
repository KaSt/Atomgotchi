# Automated Build & Release System

This document describes the automated firmware build and release system for Gotchi.

## 🔄 Overview

The Gotchi project uses GitHub Actions to automatically build, test, and release firmware for all supported devices whenever code is pushed to the main branch.

## 📋 Workflow Components

### 1. Build & Release Workflow (`.github/workflows/build-release.yml`)

**Triggers:**
- Push to `main` or `master` branch
- Manual trigger from GitHub Actions dashboard

**Process:**
1. **Prepare Release**
   - Reads version from `VERSION` file
   - Generates changelog since last release
   - Creates release notes with device list

2. **Build Firmware**
   - Compiles for 11 primary devices in parallel
   - Generates firmware binaries with version + commit hash
   - Creates manifest files for web flasher

3. **Create GitHub Release**
   - Tags repository with version (e.g., `v1.0.0`)
   - Creates GitHub release with changelog
   - Uploads all firmware binaries as release assets

4. **Update Web Flasher**
   - Copies firmware to `gh-pages` branch
   - Updates `firmware/index.json` with version info
   - Makes firmware immediately available to web flasher

### 2. GitHub Pages Deployment (`.github/workflows/deploy-pages.yml`)

**Triggers:**
- Push to `docs/flasher/**` files
- Manual trigger

**Process:**
- Deploys `docs/flasher/` to GitHub Pages
- Makes web flasher available at: `https://kast.github.io/Atomgotchi/flasher/`

## 📦 Version Management

### VERSION File

The `VERSION` file in the repository root controls releases:

```bash
# Current version
cat VERSION
# Output: 1.0.0

# Update version
echo "1.1.0" > VERSION
git add VERSION
git commit -m "Bump version to 1.1.0"
git push
```

**Automatic actions after push:**
1. ✅ Builds all firmware with new version
2. ✅ Creates git tag `v1.1.0`
3. ✅ Creates GitHub release `Gotchi v1.1.0`
4. ✅ Uploads binaries to release
5. ✅ Updates web flasher

### Manual Version Override

You can override the version when manually triggering the workflow:

1. Go to: **Actions → Build and Release Firmware → Run workflow**
2. Enter custom version (e.g., `1.2.0-beta`)
3. Click "Run workflow"

## 🏗️ Build Matrix

The workflow builds firmware for these devices in parallel:

| Device | Chip | Display | Notes |
|--------|------|---------|-------|
| M5Stack Cardputer | ESP32-S3 | 240×135 | ⭐ Recommended |
| M5StickC Plus2 | ESP32-PICO | 240×135 | Compact |
| M5Atom S3 | ESP32-S3 | 128×128 | Tiny |
| M5Atom S3R | ESP32-S3 | 128×128 | With rotary |
| M5Stack Core | ESP32 | 320×240 | Classic |
| M5Stack Core2 | ESP32 | 320×240 | Touch |
| M5Stack CoreS3 | ESP32-S3 | 320×240 | Latest |
| LilyGo T-Display S3 | ESP32-S3 | 320×170 | Popular |
| LilyGo T-Display | ESP32 | 240×135 | Original |
| LilyGo T-Embed | ESP32-S3 | 170×320 | With rotary |
| Heltec WiFi LoRa 32 V3 | ESP32-S3 | 128×64 OLED | LoRa radio |

Additional devices can be built from source using PlatformIO (26+ total).

## 📝 Changelog Generation

The workflow automatically generates changelogs by comparing commits:

```bash
# Get commits since last tag
git log v1.0.0..HEAD --pretty=format:"- %s (%h)"

# Example output:
- Add web flasher system (abc123d)
- Fix SD card detection (def456e)
- Implement AI improvements (789abcd)
```

**Changelog includes:**
- All commits since previous release
- Short commit hash for each change
- Build metadata (version, commit, timestamp)
- Installation instructions
- Device support list

## 🔧 Firmware File Naming

### Release Artifacts

```
gotchi-{board}-{version}-{commit}.bin
```

Examples:
- `gotchi-m5stack-cardputer-1.0.0-abc123d.bin`
- `gotchi-lilygo-t-display-s3-1.0.0-abc123d.bin`

### Web Flasher Files

```
gotchi-{board}-latest.bin
```

Examples:
- `gotchi-m5stack-cardputer-latest.bin`
- `gotchi-lilygo-t-display-s3-latest.bin`

The `-latest` files are automatically updated and used by the web flasher.

## 🌐 Web Flasher Integration

### Firmware Distribution

1. **Build Step:** Creates versioned firmware + "latest" symlink
2. **Release Step:** Uploads to GitHub Releases
3. **Update Step:** Copies to `gh-pages` branch under `flasher/firmware/`

### Manifest Files

Each device gets a manifest file for the web flasher:

```json
{
  "name": "Gotchi - M5Stack Cardputer",
  "version": "1.0.0",
  "commit": "abc123d",
  "board": "m5stack-cardputer",
  "chip": "esp32s3",
  "builds": [
    {
      "chipFamily": "esp32s3",
      "parts": [
        {
          "path": "gotchi-m5stack-cardputer-1.0.0-abc123d.bin",
          "offset": 0
        }
      ]
    }
  ]
}
```

### Firmware Index

The `firmware/index.json` file tracks available firmware:

```json
{
  "version": "1.0.0",
  "commit": "abc123d",
  "updated": "2025-11-23T00:45:00Z",
  "manifests": [
    "manifest-m5stack-cardputer.json",
    "manifest-lilygo-t-display-s3.json",
    ...
  ]
}
```

## 🚀 Manual Release Workflow

### Standard Release

```bash
# 1. Update version
echo "1.1.0" > VERSION

# 2. Commit changes
git add VERSION
git commit -m "Release v1.1.0"

# 3. Push to trigger build
git push origin main

# Wait ~15 minutes for:
# - All firmware to build
# - Release to be created
# - Web flasher to update
```

### Hotfix Release

```bash
# 1. Create hotfix version
echo "1.0.1" > VERSION

# 2. Commit fix
git add VERSION src/fixed-file.cpp
git commit -m "Hotfix: Critical bug in feature X"

# 3. Push
git push origin main

# Release created automatically
```

### Beta/RC Release

Use manual trigger with version override:

1. Go to **Actions → Build and Release Firmware**
2. Click **Run workflow**
3. Enter version: `1.2.0-beta1` or `1.2.0-rc1`
4. Click **Run workflow**

## 📊 Build Status & Artifacts

### Viewing Build Status

- **Repository:** Check green checkmark next to commit
- **Actions Tab:** View detailed logs for each build
- **Releases:** See published releases

### Downloading Artifacts

**From GitHub Actions (during development):**
1. Go to Actions → Select workflow run
2. Scroll to "Artifacts" section
3. Download `firmware-{board}` zip files

**From Releases (for users):**
1. Go to Releases
2. Click on latest release
3. Download desired `.bin` file from Assets

## 🔐 Security & Verification

### Build Reproducibility

- All builds happen in clean GitHub-hosted runners
- Same PlatformIO version and dependencies
- Deterministic build flags

### Checksums

Each release includes SHA-256 checksums in release notes:

```bash
# Verify downloaded firmware
sha256sum gotchi-m5stack-cardputer-1.0.0-abc123d.bin

# Compare with release notes checksum
```

### Source Verification

All firmware can be verified by:
1. Checking out the tagged commit
2. Building locally with PlatformIO
3. Comparing binary checksums

## 🐛 Troubleshooting

### Build Fails

**Check:**
- PlatformIO configuration in `platformio.ini`
- Library dependencies are available
- Build flags are correct for each board

**Debug:**
```bash
# Test build locally
pio run -e m5stack-cardputer -v
```

### Release Not Created

**Possible causes:**
- Build failed for one or more devices
- Git tag already exists
- Permissions issue with `GITHUB_TOKEN`

**Fix:**
- Check Actions logs for errors
- Delete existing tag: `git push origin :refs/tags/v1.0.0`
- Re-run workflow

### Web Flasher Not Updated

**Check:**
- `gh-pages` branch exists
- Workflow has write permissions
- `docs/flasher/firmware/` directory structure

**Manual update:**
```bash
git checkout gh-pages
# Copy firmware files manually
git add flasher/firmware/
git commit -m "Manual firmware update"
git push
```

## 📚 Related Documentation

- [PlatformIO Configuration](../platformio.ini)
- [Web Flasher Documentation](../docs/flasher/README.md)
- [Main README](../README.md)
- [Contributing Guide](../CONTRIBUTING.md) (if exists)

## 🎯 Future Improvements

- [ ] Add automated testing before release
- [ ] Generate device comparison matrix
- [ ] Create installation video tutorials
- [ ] Add firmware size optimization reports
- [ ] Implement beta/stable release channels
- [ ] Add OTA update capability
- [ ] Generate API documentation
