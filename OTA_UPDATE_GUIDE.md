# 🚀 Over-The-Air (OTA) Update System

## Overview

Your ESP32-S3 now supports secure Over-The-Air firmware updates via MQTT notifications. This allows you to remotely update devices in the field without physical access.

## 🏗️ System Architecture

```
[Firmware Server] → [MQTT Broker] → [ESP32 Device]
        ↓                ↓              ↓
   Hosts .bin files   Notifications   Downloads & Updates
```

### Components

1. **ESP32 Firmware** (v1.3.0+) - Receives updates, downloads, validates, and installs
2. **MQTT Broker** (EMQX) - Delivers update notifications  
3. **Firmware Server** - Hosts firmware files (HTTP/HTTPS)
4. **Update Tools** - Scripts to trigger and manage updates

## 🔐 Security Features

- **SHA256 Signature Validation** - Prevents installation of corrupted/malicious firmware
- **Version Comparison** - Only updates to newer versions (unless forced)
- **HTTPS Support** - Secure firmware downloads
- **Rollback Protection** - Failed updates don't brick the device
- **JWT Authentication** - Secure MQTT communication

## 📋 Quick Start

### 1. **Prepare Firmware**

Build your updated firmware:
```bash
pio run
```

Calculate firmware hash:
```bash
cd ota_tools
python3 generate_firmware_hash.py ../.pio/build/esp32s3/firmware.bin
```

### 2. **Start Firmware Server**

```bash
cd ota_tools
python3 firmware_server.py
```

Copy your firmware file to `./firmware/` directory.

### 3. **Send Update Notification**

```bash
python3 send_ota_update.py 1.3.0 http://localhost:8080/firmware/firmware.bin
```

### 4. **Monitor Update Process**

Watch ESP32 serial output for update progress:
```
Received firmware update message
Starting OTA update process...
OTA Status: downloading - Downloading firmware...
Progress: 25%
Progress: 50%
Progress: 75%
Progress: 100%
OTA Status: validating - Validating firmware signature...
OTA Status: installing - Installing firmware...
OTA Update successful! Rebooting...
```

## 📡 MQTT Topics

### Update Notification Topic
```
sensor_XXXXXX/firmware/update
```

**Message Format:**
```json
{
  "version": "1.3.0",
  "firmware_url": "https://your-server.com/firmware/esp32_v1.3.0.bin",
  "signature": "a1b2c3d4e5f6...",
  "force_update": false,
  "description": "Bug fixes and new features"
}
```

### Status Reporting Topic
```
sensor_XXXXXX/ota/status
```

**Status Messages:**
```json
{
  "device_id": "esp32s3_sensor_01",
  "firmware_version": "1.2.0",
  "ota_status": "downloading",
  "message": "Progress: 45%",
  "timestamp": 1234567890,
  "free_heap": 280120
}
```

## 🛠️ Update Process Flow

1. **Notification Received** - ESP32 receives MQTT update message
2. **Version Check** - Compare new version with current
3. **Download** - Fetch firmware from HTTP/HTTPS URL
4. **Validation** - Verify SHA256 signature
5. **Installation** - Flash new firmware to update partition
6. **Reboot** - Restart with new firmware
7. **Rollback** - Automatic rollback if new firmware fails to boot

## 🔧 Configuration

### Update ESP32 Configuration

```cpp
// In main.cpp, update these values:
const char* OTA_SERVER_URL = "https://your-firmware-server.com";
const char* OTA_PUBLIC_KEY = "-----BEGIN PUBLIC KEY-----\n..."; // For RSA signatures
```

### Update Server Configuration

Edit `ota_tools/send_ota_update.py`:
```python
MQTT_SERVER = "192.168.1.48"
MQTT_USERNAME = "YOUR_DEVICE_MAC"
DEVICE_HOSTNAME = "sensor_XXXXXX"
```

## 🔒 Production Security

### 1. **RSA/ECDSA Signatures** (Recommended)

Replace SHA256 hash validation with proper digital signatures:

```bash
# Generate key pair
openssl genrsa -out private_key.pem 2048
openssl rsa -in private_key.pem -pubout -out public_key.pem

# Sign firmware
openssl dgst -sha256 -sign private_key.pem -out firmware.sig firmware.bin

# Convert signature to base64
base64 firmware.sig > firmware.sig.b64
```

### 2. **HTTPS Certificate Validation**

Configure proper CA certificates in ESP32:
```cpp
client.setCACert(root_ca);  // Instead of setInsecure()
```

### 3. **Secure Key Storage**

Store private keys securely and rotate regularly.

## 📊 Monitoring & Logging

### ESP32 Status Monitoring

Subscribe to status topics:
```bash
mosquitto_sub -h 192.168.1.48 -p 1883 -u YOUR_MAC -P YOUR_JWT -t "sensor_+/ota/status"
```

### Update Success Tracking

Monitor device reconnections with new firmware versions in EMQX dashboard.

## 🔄 Update Scenarios

### 1. **Normal Update**
- Device checks version
- Downloads if newer
- Validates and installs
- Reports success

### 2. **Forced Update**
- Bypasses version check
- Useful for security patches
- Downloads and installs regardless

### 3. **Failed Update**
- Download fails → Retry later
- Validation fails → Reject update
- Installation fails → Rollback to previous version

## 🛡️ Rollback Protection

ESP32 implements automatic rollback:
- New firmware has 30 seconds to mark itself as valid
- If validation fails, automatically rolls back
- Prevents "bricking" devices with bad firmware

## 📈 Fleet Management

### Batch Updates

Update multiple devices:
```bash
# Update all sensors in a location
for device in sensor_001 sensor_002 sensor_003; do
    python3 send_ota_update.py 1.3.0 https://server.com/firmware.bin --device $device
done
```

### Staged Rollouts

1. **Canary** - Update 1-2 devices first
2. **Beta** - Update 10% of fleet
3. **Production** - Update remaining devices

## 🐛 Troubleshooting

### Common Issues

**Update Stuck at "Downloading"**
- Check firmware server is running
- Verify URL is accessible from ESP32
- Check network connectivity

**"Signature validation failed"**
- Verify SHA256 hash is correct
- Ensure firmware file wasn't corrupted
- Check signature calculation method

**"Installation failed"**
- Insufficient flash space
- Corrupted download
- Hardware issues

### Debug Commands

```bash
# Test firmware hash
python3 generate_firmware_hash.py firmware.bin

# Test server connectivity  
curl -I http://localhost:8080/firmware/firmware.bin

# Monitor MQTT messages
mosquitto_sub -h 192.168.1.48 -p 1883 -u MAC -P JWT -t "sensor_+/ota/status"
```

## 📝 Version Management

### Semantic Versioning

Use semantic versioning (MAJOR.MINOR.PATCH):
- **1.3.0** → **1.3.1** (patch: bug fixes)
- **1.3.0** → **1.4.0** (minor: new features)
- **1.3.0** → **2.0.0** (major: breaking changes)

### Release Process

1. Update `FIRMWARE_VERSION` in main.cpp
2. Build and test firmware
3. Generate SHA256 hash
4. Upload to firmware server
5. Send update notifications
6. Monitor deployment

## 🚀 Future Enhancements

- **Differential Updates** - Only download changed parts
- **Scheduled Updates** - Update during maintenance windows
- **Update Policies** - Auto-update rules based on criticality
- **Binary Patches** - Reduce download size
- **A/B Partitioning** - Instant rollback capability

---

Your ESP32 devices now have enterprise-grade OTA update capabilities! 🎉
