# Firmware Version Changelog

## Version 1.2.0 - Current
**Release Date**: TBD (Auto-generated build timestamp)

### ✅ New Features
- **Version Tracking**: Added semantic versioning to client ID and status messages
- **Build Timestamp**: Automatic compilation timestamp for precise version identification
- **Enhanced Telemetry**: Firmware version and build info included in status messages and JWT tokens
- **Improved Client ID**: Format now includes version: `sensor_8EA40C_v1.2.0_1234`

### ✅ Bug Fixes & Improvements
- **MQTT Stability**: Fixed disconnection issues with unique client IDs
- **JWT Authentication**: Corrected issuer claim from "esp32-device" to "esp32-sensor"
- **Buffer Management**: Increased MQTT buffer size to 1024 bytes for large JWT tokens
- **Network Reliability**: Added 2-second WiFi stabilization delay
- **Connection Timeouts**: Set 60s keepalive and 30s socket timeout
- **Auto JWT Refresh**: Automatic token renewal every 30 minutes
- **Enhanced Debugging**: More detailed connection status and error reporting

### 🔧 Technical Details
- **Client ID Format**: `{hostname}_v{version}_{random4digits}`
- **JWT Payload**: Now includes firmware_version and build_timestamp
- **Status Messages**: Include firmware_version, build_timestamp, and free_heap
- **Memory Usage**: Enhanced heap monitoring and reporting

### 📊 Telemetry Data Added
```json
{
  "firmware_version": "1.2.0",
  "build_timestamp": "Oct 18 2025 14:30:22",
  "free_heap": 280120
}
```

---

## Version 1.1.0 - Previous
**Release Date**: Prior to version tracking

### Features
- Basic MQTT connectivity with JWT authentication
- Temperature sensor reading and publishing
- Light control via MQTT commands
- WiFi connection management
- NTP time synchronization
- LED status indicators

### Known Issues (Fixed in 1.2.0)
- MQTT disconnection issues
- JWT issuer claim mismatch
- Client ID conflicts causing connection drops
- Limited debugging information

---

## Version 1.0.0 - Initial
**Release Date**: Project inception

### Features
- Initial ESP32-S3 support
- Basic MQTT client implementation
- Temperature sensor integration
- WiFi connectivity
- LED control

---

## Upgrading

To identify your current firmware version:

1. **Serial Monitor**: Check startup messages for version info
2. **MQTT Client ID**: Look for version in connection logs on EMQX dashboard
3. **Status Messages**: Version info included in `{hostname}/status` topic

## Future Versions

### Planned for 1.3.0
- [ ] Over-The-Air (OTA) firmware updates
- [ ] Web configuration interface
- [ ] Multiple sensor support
- [ ] Data logging and caching
- [ ] TLS/SSL MQTT encryption

### Planned for 1.4.0
- [ ] Sensor calibration interface
- [ ] Alert thresholds and notifications
- [ ] Power management optimizations
- [ ] Watchdog timer implementation

---

## Version Numbering Scheme

Following Semantic Versioning (SemVer):
- **MAJOR.MINOR.PATCH** (e.g., 1.2.0)
- **MAJOR**: Breaking changes or major feature additions
- **MINOR**: New features, backward compatible
- **PATCH**: Bug fixes, minor improvements

## Build Information

Each build includes:
- **Compilation Date**: Exact build timestamp
- **Version Hash**: Based on git commit (future feature)
- **Board Configuration**: ESP32-S3 specific settings
- **Library Versions**: Dependency tracking (future feature)
