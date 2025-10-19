# 🔧 XIAO-ESP32-S3 LED Pin Alternatives

If the current pin assignments aren't working, try these alternatives:

## **Option 1: Conservative Pins**
```cpp
const int LED_PIN = 21;          // GPIO21 - Should be built-in LED
const int NEOPIXEL_PIN = 1;      // GPIO1 - D0 pin
```

## **Option 2: Different Pins**
```cpp
const int LED_PIN = 2;           // GPIO2 - Alternative LED pin
const int NEOPIXEL_PIN = 3;      // GPIO3 - D2 pin
```

## **Option 3: Safe Pins**
```cpp
const int LED_PIN = 21;          // GPIO21 - Built-in LED
const int NEOPIXEL_PIN = 4;      // GPIO4 - D3 pin
```

To test, update the pin assignments in `src/main.cpp` and re-upload:
```bash
pio run --target upload
```

## **XIAO-ESP32-S3 Pinout Reference:**
- **D0**: GPIO1
- **D1**: GPIO2  
- **D2**: GPIO3
- **D3**: GPIO4
- **D4**: GPIO5
- **D5**: GPIO6
- **A0**: GPIO2 (same as D1)
- **Built-in LED**: GPIO21

The current setup uses:
- **Built-in LED**: GPIO21 (`LED_BUILTIN`)
- **Neopixels**: GPIO2 (A0 pin)
