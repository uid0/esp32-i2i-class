# 📌 Seeed Studio XIAO-ESP32-S3 Pinout Guide

## Board Overview

The **Seeed Studio XIAO-ESP32-S3** is a compact development board with limited but well-designed pin layout.

## 🔌 Pin Assignments Used

### **Current Configuration**
```cpp
const int TEMP_SENSOR_PIN = D0;    // GPIO1 - Temperature sensor
const int LIGHT_PIN = D1;          // GPIO2 - Light control
const int LED_PIN = LED_BUILTIN;   // GPIO21 - Built-in user LED  
const int NEOPIXEL_PIN = A0;       // GPIO2 - 60-LED Neopixel string
const int NUM_LEDS = 60;
```

## 📍 XIAO-ESP32-S3 Pinout Map

```
                    XIAO-ESP32-S3
                   ┌─────────────┐
                   │    USB-C    │
                   └─────────────┘
    3V3  ●─────────┤             ├─────────● GND
    GND  ●─────────┤             ├─────────● VBUS
     D0  ●─────────┤   ESP32-S3  ├─────────● D10/A10
     D1  ●─────────┤             ├─────────● D9/A9  
     D2  ●─────────┤   RISC-V    ├─────────● D8/A8
     D3  ●─────────┤             ├─────────● D7/A7
     D4  ●─────────┤   WiFi+BT   ├─────────● D6/A6
     D5  ●─────────┤             ├─────────● A0
                   │    Antenna  │
                   └─────────────┘
```

## 🎯 GPIO Mapping

| Pin Label | GPIO | Function | Type | Notes |
|-----------|------|----------|------|-------|
| **D0** | GPIO1 | Digital I/O | ✅ | Temperature sensor |
| **D1** | GPIO2 | Digital I/O | ✅ | Light control |
| **D2** | GPIO3 | Digital I/O | ✅ | Available |
| **D3** | GPIO4 | Digital I/O | ✅ | Available |
| **D4** | GPIO5 | Digital I/O | ✅ | Available |
| **D5** | GPIO6 | Digital I/O | ✅ | Available |
| **D6/A6** | GPIO7 | Analog/Digital | ✅ | Available |
| **D7/A7** | GPIO8 | Analog/Digital | ✅ | Available |
| **D8/A8** | GPIO9 | Analog/Digital | ✅ | Available |
| **D9/A9** | GPIO10 | Analog/Digital | ✅ | Available |
| **D10/A10** | GPIO11 | Analog/Digital | ✅ | Available |
| **A0** | GPIO2 | Analog/Digital | ✅ | **Neopixel data** |

### **Special Pins**
| Function | GPIO | Notes |
|----------|------|-------|
| **Built-in LED** | GPIO21 | User programmable LED |
| **USB D-** | GPIO19 | USB communication |
| **USB D+** | GPIO20 | USB communication |
| **Boot Button** | GPIO0 | Boot mode selection |

## 🌈 LED Configuration

### **Built-in LED**
- **Pin**: GPIO21 (`LED_BUILTIN`)
- **Type**: Single color LED
- **Control**: Digital HIGH/LOW
- **Location**: On the board near USB connector

### **Neopixel Strip**
- **Pin**: A0 (GPIO2)
- **Type**: WS2812B compatible
- **Count**: 60 LEDs
- **Power**: 5V external recommended

## 🔌 Hardware Connections

### **Temperature Sensor (DS18B20)**
```
XIAO-ESP32-S3 → DS18B20
---------------------
D0 (GPIO1)    → Data
3V3           → VCC
GND           → GND
```
**Note**: Add 4.7kΩ pull-up resistor between VCC and Data

### **Neopixel Strip**
```
XIAO-ESP32-S3 → Neopixel Strip
-----------------------------
A0 (GPIO2)    → Data In (DIN)
VBUS (5V)     → VCC
GND           → GND
```

### **Light Control**
```
XIAO-ESP32-S3 → LED/Relay
----------------------------
D1 (GPIO2)    → Control pin
GND           → GND
```

## ⚡ Power Considerations

### **XIAO-ESP32-S3 Power**
- **USB-C**: 5V input, 500mA typical
- **3V3 Output**: 600mA max
- **VBUS**: Direct 5V from USB

### **Neopixel Power Requirements**
- **Single LED**: ~60mA at full brightness
- **60 LEDs**: ~3.6A at full brightness
- **USB Limitation**: ~500mA total
- **Solution**: Use external 5V power supply

### **Power Distribution**
```
USB-C 5V → XIAO Board (3.3V logic)
        → VBUS Pin → Neopixel VCC (5V)
```

## 🎨 LED Status Indicators

### **Built-in LED (GPIO21)**
- **WiFi Connecting**: Slow blink (1s)
- **MQTT Connecting**: Fast blink (250ms) 
- **Connected**: Solid ON
- **OTA Update**: Very fast blink
- **Error**: Rapid blink

### **Neopixel Strip (A0/GPIO2)**
- **Startup**: Rainbow sweep animation
- **WiFi**: Blue blinking
- **MQTT**: Yellow blinking  
- **Connected**: Green solid
- **Activity**: Purple pulse
- **OTA Progress**: Orange progress bar
- **Temperature**: White flash

## 🔧 Alternative Pin Configurations

### **Option 1: More GPIOs**
```cpp
const int TEMP_SENSOR_PIN = D2;    // GPIO3
const int LIGHT_PIN = D3;          // GPIO4
const int NEOPIXEL_PIN = D4;       // GPIO5
```

### **Option 2: Analog Pins**
```cpp
const int TEMP_SENSOR_PIN = D6;    // GPIO7 (A6)
const int LIGHT_PIN = D7;          // GPIO8 (A7)
const int NEOPIXEL_PIN = D8;       // GPIO9 (A8)
```

## 🐛 Troubleshooting

### **LEDs Not Working**
1. **Check Pin Numbers**: Ensure using XIAO-specific pins
2. **Power Supply**: 60 LEDs need external power
3. **Wiring**: Verify connections
4. **Code**: Use `LED_BUILTIN` for user LED

### **Common Issues**
- **Pin conflicts**: A0 used for both analog and Neopixel
- **Power limits**: USB can't power 60 LEDs at full brightness
- **GPIO mapping**: Different from standard ESP32 boards

### **Debug Serial Output**
```
=== Seeed XIAO-ESP32-S3 MQTT Sensor with JWT ===
Board: Seeed Studio XIAO-ESP32-S3
Built-in LED pin: 21
Neopixel pin: 2
Number of LEDs: 60
```

## 📏 Board Specifications

- **Size**: 21×17.8mm (ultra-compact)
- **MCU**: ESP32-S3FN8 
- **Flash**: 8MB
- **PSRAM**: 8MB
- **WiFi**: 802.11 b/g/n
- **Bluetooth**: BLE 5.0
- **GPIO**: 11 digital I/O pins
- **ADC**: 9 analog inputs
- **USB**: USB-C with CDC

## 🎯 Optimized for XIAO

The current configuration is now optimized for the Seeed Studio XIAO-ESP32-S3:

- ✅ Correct GPIO mappings
- ✅ Built-in LED support
- ✅ Compact pin usage
- ✅ Power-efficient design
- ✅ USB-C compatibility

Perfect for IoT projects requiring small form factor with full ESP32-S3 capabilities!
