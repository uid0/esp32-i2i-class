# 🚀 ESP32-S3 MQTT Sensor with OTA Updates

**Enterprise-grade IoT sensor with remote firmware updates**

A production-ready ESP32-S3 sensor platform with temperature monitoring, device control, JWT authentication, and **Over-The-Air (OTA) firmware updates**.

## ✨ Features

### 🌡️ **Sensor Capabilities**
- **Temperature Monitoring**: DS18B20 sensor with JSON data publishing
- **Light Control**: Remote LED control via MQTT commands
- **Real-time Data**: 30-second temperature readings with timestamps

### 🔒 **Security & Authentication**
- **JWT Authentication**: Secure MQTT connections with auto-refresh
- **Cryptographic Validation**: SHA256 firmware signature verification
- **Time Synchronization**: NTP-based secure timestamps

### 📡 **Communication**
- **MQTT Integration**: Full EMQX broker support with status reporting
- **WiFi Management**: Automatic connection with reconnection logic
- **Network Monitoring**: Connection status and diagnostic information

### 🚀 **Remote Management** (NEW!)
- **Over-The-Air Updates**: Secure remote firmware updates
- **Version Management**: Semantic versioning with build timestamps
- **Fleet Monitoring**: Real-time update status and progress reporting
- **Rollback Protection**: Automatic recovery from failed updates

### 📊 **Monitoring & Diagnostics**
- **LED Status Indicators**: Visual connection and update status
- **Detailed Logging**: Comprehensive serial output and MQTT status
- **Performance Metrics**: Memory usage and system health monitoring

## 🚀 Quick Start

### **Option 1: Complete OTA System Test**
```bash
# Test the entire system including OTA updates
./test_ota_system.sh
```

### **Option 2: Standard Firmware Upload**  
```bash
# Build and upload firmware with monitoring
./upload_and_monitor.sh
```

### **Option 3: Manual Setup**
```bash
# Clone and setup
git clone <repository-url>
cd esp32-s3-mqtt-sensor

# Build and upload
pio run --target upload --target monitor
```

### **Configuration**
Update these settings in `src/main.cpp`:
- WiFi credentials (`WIFI_SSID`, `WIFI_PASSWORD`)
- MQTT broker (`MQTT_SERVER`)
- JWT secret (`JWT_SECRET`)
- OTA server URL (`OTA_SERVER_URL`)

## ⚡ Hardware Requirements

### **Development Board**
- **ESP32-S3 DevKitC-1** (8MB Flash recommended for OTA)
- USB-C cable for programming and monitoring

### **Sensors & Components**
- **DS18B20** temperature sensor (digital, waterproof available)
- **LED** for status indication and control testing
- **4.7kΩ resistor** for DS18B20 pull-up
- **Breadboard** and jumper wires for prototyping

### **Network Infrastructure**
- **WiFi network** with internet access
- **EMQX MQTT broker** (local or cloud)
- **HTTP/HTTPS server** for firmware hosting (optional for OTA)

### **Hardware Wiring**

| Component | ESP32-S3 Pin | Notes |
|-----------|--------------|-------|
| DS18B20 Data | GPIO2 | Temperature sensor with 4.7kΩ pull-up |
| LED/Light | GPIO8 | Light control output |
| DS18B20 VCC | 3.3V | Power |
| DS18B20 GND | GND | Ground |
| Built-in LED | GPIO2 | Status indicator |

## 📡 MQTT Topics

### **Sensor Data** (Published)
- `{hostname}/sensors/temperature` - Temperature readings with metadata
- `{hostname}/status` - Device status with firmware version
- `{hostname}/light/status` - Light control status

### **Device Control** (Subscribed)
- `{hostname}/light/control` - Light control commands (`on`/`off`)
- `{hostname}/firmware/update` - OTA update notifications

### **OTA Management** (Published)
- `{hostname}/ota/status` - Update progress and status reporting

**Example hostname**: `sensor_8EA40C` (based on MAC address)

## 🔧 Technical Specifications

### **Microcontroller**
- **ESP32-S3** dual-core @ 240MHz
- **Memory**: 512KB SRAM, 8MB Flash
- **Connectivity**: WiFi 802.11 b/g/n
- **Security**: Hardware encryption support

### **Firmware**
- **Version**: 1.3.0 (with OTA support)
- **Build System**: PlatformIO
- **Memory Usage**: 14.2% RAM, 27.6% Flash
- **Update Method**: Over-The-Air (OTA) via MQTT

### **Sensors & Interfaces**
- **DS18B20**: Digital temperature (-55°C to +125°C)
- **GPIO**: Light control and status LED
- **Serial**: USB-C for monitoring and debugging
- **1-Wire**: Temperature sensor communication

### **Network & Security**
- **MQTT**: Encrypted JWT authentication
- **NTP**: Network time synchronization  
- **OTA**: SHA256 signature validation
- **TLS**: HTTPS firmware downloads

## 📁 Project Structure

```
esp32-s3-mqtt-sensor/
├── src/
│   └── main.cpp                    # Main application code with OTA
├── ota_tools/                      # OTA management tools
│   ├── generate_firmware_hash.py   # Calculate firmware signatures
│   ├── firmware_server.py          # Local firmware server
│   └── send_ota_update.py          # Send update notifications
├── docs/                           # Documentation
│   ├── OTA_UPDATE_GUIDE.md         # Complete OTA guide
│   ├── OTA_IMPLEMENTATION_SUMMARY.md # Implementation overview
│   ├── README_OTA.md               # OTA quick reference
│   ├── README_VERSIONING.md        # Version management guide
│   └── EMQX_JWT_SETUP.md          # EMQX configuration
├── scripts/
│   ├── upload_and_monitor.sh       # Upload with port handling
│   └── test_ota_system.sh          # Complete OTA system test
├── platformio.ini                  # PlatformIO configuration
└── README.md                       # This file
```

## 🎮 OTA Update Demo

### **Send a Firmware Update**
```bash
# Calculate firmware hash
cd ota_tools
python3 generate_firmware_hash.py ../.pio/build/esp32s3/firmware.bin

# Start firmware server
python3 firmware_server.py &

# Send update notification
python3 send_ota_update.py 1.3.0 http://localhost:8080/firmware/firmware.bin
```

### **Monitor Update Progress**
```bash
# Watch ESP32 serial output
pio device monitor

# Monitor MQTT status
mosquitto_sub -h 192.168.1.48 -p 1883 -u YOUR_MAC -P JWT -t "sensor_+/ota/status"
```

## 📚 Documentation

### **Setup & Configuration**
- **[EMQX_JWT_SETUP.md](EMQX_JWT_SETUP.md)** - MQTT broker JWT authentication
- **[README_VERSIONING.md](README_VERSIONING.md)** - Firmware version management

### **OTA Updates**
- **[README_OTA.md](README_OTA.md)** - OTA quick reference guide
- **[OTA_UPDATE_GUIDE.md](OTA_UPDATE_GUIDE.md)** - Complete OTA technical guide
- **[OTA_IMPLEMENTATION_SUMMARY.md](OTA_IMPLEMENTATION_SUMMARY.md)** - Implementation overview

### **Troubleshooting**
- **[SOLUTION_SUMMARY.md](SOLUTION_SUMMARY.md)** - MQTT connection troubleshooting
- **[MQTT_TROUBLESHOOTING.md](MQTT_TROUBLESHOOTING.md)** - Detailed debugging guide

## 🖥️ Example Serial Output

```
=== ESP32-S3 MQTT Sensor with JWT ===
Firmware Version: 1.3.0
Build Timestamp: Oct 18 2025 14:30:22
Device hostname: sensor_8EA40C
=== WiFi Connection ===
WiFi connected! IP: 192.168.1.100
=== Time Synchronization ===
Time synchronized!
=== MQTT Connection ===
Client ID: sensor_8EA40C_v1.3.0_5678
MQTT connected!
Subscribed to: sensor_8EA40C/light/control
Subscribed to firmware updates: sensor_8EA40C/firmware/update
```

## 🔄 OTA Update Process

When you send an OTA update, you'll see:

```
Received firmware update message
Firmware update available: 1.3.1
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

## 🛡️ Security Features

- **JWT Authentication**: Secure MQTT communication with auto-refresh
- **Firmware Validation**: SHA256 signature verification prevents malicious updates
- **Rollback Protection**: Failed updates automatically roll back
- **Version Control**: Only installs newer firmware versions
- **HTTPS Support**: Secure firmware downloads
- **Time Validation**: NTP synchronization for secure timestamps

## 🚀 Production Deployment

### **Fleet Management**
- Update multiple devices simultaneously
- Staged rollouts with canary deployments
- Real-time monitoring of update success rates
- Emergency update capability for security patches

### **Monitoring Integration**
- MQTT status reporting for all devices
- Integration with monitoring dashboards
- Automated alerting on failed updates
- Historical deployment tracking

## 🏆 What You Get

This project provides:

- ⚡ **Remote Updates**: Update devices anywhere in the world
- 🔒 **Enterprise Security**: Production-grade authentication and validation
- 📊 **Complete Monitoring**: Real-time device status and update progress
- 🛡️ **Fault Tolerance**: Devices never get "bricked" by bad updates
- 🎯 **Version Control**: Smart deployment management
- 🚀 **Scalable Architecture**: Ready for hundreds or thousands of devices

---

**Your ESP32-S3 devices now have the same update capabilities as major IoT platforms!** 🎊

**Version**: 1.3.0 | **OTA**: Fully Implemented ✅ | **Ready for Production**: 🚀