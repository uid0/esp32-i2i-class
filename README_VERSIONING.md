# 🏷️ Firmware Versioning System

## Overview

Your ESP32-S3 firmware now includes a comprehensive versioning system that will help you track and identify firmware versions in production.

## ✅ What's Been Added

### 1. **Semantic Version Number**
```cpp
#define FIRMWARE_VERSION "1.2.0"
```
- Current version: **1.2.0**
- Follows Semantic Versioning (MAJOR.MINOR.PATCH)
- Easy to update for new releases

### 2. **Build Timestamp**
```cpp
#define BUILD_TIMESTAMP __DATE__ " " __TIME__
```
- Automatically generated at compile time
- Example: `"Oct 18 2025 14:30:22"`
- Exact build identification

### 3. **Enhanced Client ID**
**Old format**: `sensor_8EA40C_1234567`  
**New format**: `sensor_8EA40C_v1.2.0_5678`

- Includes version number in MQTT client ID
- Visible in EMQX dashboard connections
- Helps identify firmware version from broker logs

### 4. **Version Info in Serial Output**
```
=== ESP32-S3 MQTT Sensor with JWT ===
Firmware Version: 1.2.0
Build Timestamp: Oct 18 2025 14:30:22
```

### 5. **Enhanced Status Messages**
Status messages now include:
```json
{
  "device_id": "esp32s3_sensor_01",
  "status": "online",
  "firmware_version": "1.2.0",
  "build_timestamp": "Oct 18 2025 14:30:22",
  "free_heap": 280120,
  "ip_address": "192.168.1.100",
  "mac_address": "8C:BF:EA:8E:A4:0C"
}
```

### 6. **Version Info in JWT Tokens**
JWT authentication tokens now include:
```json
{
  "iss": "esp32-sensor",
  "firmware_version": "1.2.0",
  "build_timestamp": "Oct 18 2025 14:30:22",
  ...
}
```

## 🚀 Upload the New Firmware

### Option 1: Use the Upload Script
```bash
./upload_and_monitor.sh
```

### Option 2: Manual Upload
```bash
# Close any serial monitors first
pio run --target upload
pio device monitor
```

## 📊 Monitoring and Identification

### 1. **Serial Monitor**
When the ESP32 boots, you'll see:
```
=== ESP32-S3 MQTT Sensor with JWT ===
Firmware Version: 1.2.0
Build Timestamp: Oct 18 2025 14:30:22
Client ID: sensor_8EA40C_v1.2.0_5678
```

### 2. **EMQX Dashboard**
- Go to `http://192.168.1.48:18083`
- **Monitoring** → **Connections**
- Look for client ID with version: `sensor_8EA40C_v1.2.0_XXXX`

### 3. **MQTT Status Messages**
Subscribe to: `sensor_8EA40C/status`
```json
{
  "firmware_version": "1.2.0",
  "build_timestamp": "Oct 18 2025 14:30:22"
}
```

## 🔄 Future Version Updates

### To Release Version 1.2.1 (Patch):
```cpp
#define FIRMWARE_VERSION "1.2.1"
```

### To Release Version 1.3.0 (Minor):
```cpp
#define FIRMWARE_VERSION "1.3.0"
```

### To Release Version 2.0.0 (Major):
```cpp
#define FIRMWARE_VERSION "2.0.0"
```

## 🎯 Benefits

### 1. **Easy Troubleshooting**
- Instantly identify firmware version from logs
- Correlate issues with specific builds
- Track deployment status across devices

### 2. **Production Monitoring**
- Monitor firmware distribution in fleet
- Identify devices needing updates
- Track performance by version

### 3. **Development Tracking**
- Link issues to specific builds
- Verify successful deployments
- Historical change tracking

### 4. **EMQX Dashboard Visibility**
- Version visible in connection list
- Easy filtering by firmware version
- Deployment status at a glance

## 📝 Version Log Example

After deployment, you'll see entries like:
```
[2025-10-18 14:30:22] sensor_8EA40C_v1.2.0_5678 connected
[2025-10-18 14:30:23] Status: {"firmware_version": "1.2.0", "status": "online"}
```

## 🛠️ Changelog

See `VERSION_CHANGELOG.md` for complete version history and upgrade notes.

---

**Next Steps:**
1. Upload the new firmware: `./upload_and_monitor.sh`
2. Verify version info in serial output
3. Check EMQX dashboard for versioned client ID
4. Monitor status messages for version tracking

Your ESP32 now has enterprise-grade version tracking! 🎉
