# 🚀 OTA Update System - Quick Reference

## What's New in Version 1.3.0

Your ESP32-S3 firmware now includes a complete **Over-The-Air (OTA) update system**! You can now remotely update devices without physical access.

## ⚡ Quick Test

Run the complete OTA system test:
```bash
./test_ota_system.sh
```

This will:
1. ✅ Build firmware and calculate hash
2. 🌐 Start local firmware server
3. 📤 Send update notification to ESP32
4. 📊 Monitor the update process

## 🎯 Key Features

### ✅ **Secure Updates**
- SHA256 signature validation
- HTTPS support for firmware downloads
- JWT-authenticated MQTT notifications
- Automatic rollback on failure

### ✅ **Smart Version Management**
- Semantic version comparison (1.2.0 vs 1.3.0)
- Force update capability for emergency patches
- Version info embedded in client ID and messages

### ✅ **Real-time Monitoring**
- Progress reporting during download
- Status updates via MQTT
- Detailed error messages and debugging

### ✅ **Production Ready**
- Robust error handling and recovery
- Memory-efficient streaming downloads
- Automatic retry mechanisms

## 📋 Quick Commands

### Build and Upload Current Firmware
```bash
./upload_and_monitor.sh
```

### Calculate Firmware Hash
```bash
cd ota_tools
python3 generate_firmware_hash.py ../firmware.bin
```

### Start Firmware Server
```bash
cd ota_tools
python3 firmware_server.py
```

### Send OTA Update
```bash
cd ota_tools
python3 send_ota_update.py 1.3.0 http://localhost:8080/firmware/firmware.bin
```

### Monitor OTA Status
```bash
mosquitto_sub -h 192.168.1.48 -p 1883 -u YOUR_MAC -P YOUR_JWT -t "sensor_+/ota/status"
```

## 🔄 Update Process

1. **📦 Build Firmware** → Compile new version
2. **🧮 Calculate Hash** → Generate SHA256 signature  
3. **🌐 Upload to Server** → Make firmware accessible via HTTP
4. **📤 Send MQTT Notification** → Trigger device update
5. **⬇️ Device Downloads** → ESP32 fetches firmware
6. **🔍 Validate Signature** → Verify integrity
7. **⚡ Install & Reboot** → Apply update automatically

## 📡 MQTT Topics

| Topic | Purpose | Direction |
|-------|---------|-----------|
| `sensor_XXXX/firmware/update` | Update notifications | Server → Device |
| `sensor_XXXX/ota/status` | Status reporting | Device → Server |

## 🛠️ Configuration

Update these values in your deployment:

**ESP32 Firmware** (`main.cpp`):
```cpp
const char* OTA_SERVER_URL = "https://your-server.com";
```

**Update Tools** (`send_ota_update.py`):
```python
MQTT_SERVER = "192.168.1.48"
DEVICE_HOSTNAME = "sensor_XXXX"  # Your device
```

## 🔐 Security Notes

### Current Implementation (Development)
- ✅ SHA256 hash validation
- ⚠️ HTTPS with `setInsecure()` (development only)
- ✅ JWT authentication for MQTT

### Production Recommendations
- 🔒 Implement proper RSA/ECDSA signatures
- 🔒 Configure proper SSL/TLS certificates
- 🔒 Use secure key storage and rotation
- 🔒 Enable certificate validation

## 📊 Monitoring

### ESP32 Serial Output
```
=== Starting OTA Update ===
Downloading from: http://localhost:8080/firmware/firmware.bin
Firmware size: 741248 bytes
Progress: 25%
Progress: 50%
Progress: 75%
Progress: 100%
Download completed: 741248 bytes
Firmware signature validated successfully
OTA Update successful! Rebooting...
```

### MQTT Status Messages
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

## 🐛 Common Issues

**"Download failed"**
- Check network connectivity
- Verify firmware server is running
- Ensure URL is accessible from ESP32

**"Signature validation failed"**
- Verify SHA256 hash is correct
- Check firmware file integrity
- Ensure hash calculation matches

**"Installation failed"**
- Insufficient flash memory
- Corrupted firmware file
- Hardware issues

## 📚 Complete Documentation

- **[OTA_UPDATE_GUIDE.md](OTA_UPDATE_GUIDE.md)** - Complete technical guide
- **[VERSION_CHANGELOG.md](VERSION_CHANGELOG.md)** - Version history
- **[ota_tools/](ota_tools/)** - Management scripts and server

## 🚀 Get Started

1. **Test the system**: `./test_ota_system.sh`
2. **Monitor ESP32**: Connect serial monitor
3. **Watch the magic**: Firmware updates automatically!

Your ESP32 devices now have enterprise-grade remote update capabilities! 🎉

---

**Version**: 1.3.0 | **Build**: Auto-generated | **OTA**: Enabled ✅
