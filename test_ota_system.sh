#!/bin/bash

# Complete OTA System Test Script
# This script demonstrates the full OTA update workflow

echo "🚀 ESP32 OTA Update System Test"
echo "================================"

# Configuration
FIRMWARE_VERSION="1.3.0"
DEVICE_MAC="8CBFEA8EA40C"
DEVICE_HOSTNAME="sensor_8EA40C"

# Check if firmware file exists
FIRMWARE_FILE=".pio/build/esp32s3/firmware.bin"

if [ ! -f "$FIRMWARE_FILE" ]; then
    echo "❌ Firmware file not found: $FIRMWARE_FILE"
    echo "🔧 Building firmware first..."
    pio run
    
    if [ ! -f "$FIRMWARE_FILE" ]; then
        echo "❌ Build failed. Cannot proceed with OTA test."
        exit 1
    fi
fi

echo "✅ Firmware file found: $FIRMWARE_FILE"

# Step 1: Calculate firmware hash
echo ""
echo "Step 1: Calculating firmware hash..."
echo "-----------------------------------"
cd ota_tools
python3 generate_firmware_hash.py "../$FIRMWARE_FILE"

if [ $? -ne 0 ]; then
    echo "❌ Hash calculation failed"
    exit 1
fi

# Step 2: Start firmware server (in background)
echo ""
echo "Step 2: Starting firmware server..."
echo "----------------------------------"

# Copy firmware to server directory
mkdir -p firmware
cp "../$FIRMWARE_FILE" "firmware/esp32s3_firmware_v${FIRMWARE_VERSION}.bin"

# Start server in background
python3 firmware_server.py &
SERVER_PID=$!

echo "✅ Firmware server started (PID: $SERVER_PID)"
echo "🌐 Server URL: http://localhost:8080"

# Wait for server to start
sleep 3

# Test server
echo "🧪 Testing server connectivity..."
curl -s -I "http://localhost:8080/api/version" > /dev/null

if [ $? -eq 0 ]; then
    echo "✅ Server is responding"
else
    echo "❌ Server not responding"
    kill $SERVER_PID 2>/dev/null
    exit 1
fi

# Step 3: Prepare update message
echo ""
echo "Step 3: Preparing update message..."
echo "---------------------------------"

FIRMWARE_URL="http://localhost:8080/firmware/esp32s3_firmware_v${FIRMWARE_VERSION}.bin"
echo "📦 Firmware URL: $FIRMWARE_URL"

# Step 4: Send OTA update (interactive)
echo ""
echo "Step 4: Send OTA update to ESP32..."
echo "---------------------------------"
echo "🔔 This will send an update notification to your ESP32 device"
echo "📋 Make sure your ESP32 is connected and monitoring serial output"
echo ""

read -p "Proceed with sending OTA update? (y/N): " -n 1 -r
echo

if [[ $REPLY =~ ^[Yy]$ ]]; then
    echo "📤 Sending OTA update notification..."
    python3 send_ota_update.py "$FIRMWARE_VERSION" "$FIRMWARE_URL" "firmware/esp32s3_firmware_v${FIRMWARE_VERSION}.bin"
    
    if [ $? -eq 0 ]; then
        echo ""
        echo "✅ OTA update notification sent successfully!"
        echo ""
        echo "🖥️  Next Steps:"
        echo "   1. Monitor ESP32 serial output for update progress"
        echo "   2. Watch for download progress messages"
        echo "   3. Device will reboot automatically after successful update"
        echo "   4. Verify new firmware version in startup messages"
        echo ""
        echo "📊 Monitor update status:"
        echo "   mosquitto_sub -h 192.168.1.48 -p 1883 -u $DEVICE_MAC -P JWT_TOKEN -t \"$DEVICE_HOSTNAME/ota/status\""
    else
        echo "❌ Failed to send OTA update notification"
    fi
else
    echo "Update cancelled by user"
fi

# Cleanup
echo ""
echo "🧹 Cleanup..."
echo "------------"

# Keep server running for a while to allow ESP32 to download
echo "Server will run for 60 more seconds to allow firmware download..."
echo "Press Ctrl+C to stop server immediately"

# Wait for download or user interrupt
trap "echo; echo '🛑 Stopping server...'; kill $SERVER_PID 2>/dev/null; exit 0" INT

sleep 60

echo "⏰ Stopping server after timeout..."
kill $SERVER_PID 2>/dev/null

echo ""
echo "✅ OTA system test completed!"
echo ""
echo "🔍 Troubleshooting:"
echo "   - Check ESP32 serial output for error messages"
echo "   - Verify network connectivity between ESP32 and this machine"
echo "   - Ensure MQTT broker is running and accessible"
echo "   - Check firmware file integrity and hash"

cd ..
