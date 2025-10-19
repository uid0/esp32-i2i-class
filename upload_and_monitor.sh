#!/bin/bash

# ESP32-S3 Upload and Monitor Script
# This script handles port conflicts and provides clean upload/monitoring

echo "🚀 ESP32-S3 Firmware Upload and Monitor"
echo "========================================"

# Kill any existing serial monitors
echo "📡 Closing any existing serial connections..."
pkill -f "screen.*usbmodem"
pkill -f "minicom.*usbmodem"
sleep 2

# Upload firmware
echo "📤 Uploading firmware to ESP32-S3..."
pio run --target upload

if [ $? -eq 0 ]; then
    echo "✅ Upload successful!"
    echo ""
    echo "🖥️  Starting serial monitor..."
    echo "   Press Ctrl+A then K to exit"
    echo "   Or Ctrl+C to stop this script"
    echo ""
    sleep 2
    
    # Start serial monitor
    pio device monitor
else
    echo "❌ Upload failed!"
    echo ""
    echo "🔧 Troubleshooting:"
    echo "   1. Check USB cable connection"
    echo "   2. Press and hold BOOT button on ESP32, then press RESET"
    echo "   3. Release BOOT button and try again"
    echo "   4. Ensure no other programs are using the serial port"
    exit 1
fi
