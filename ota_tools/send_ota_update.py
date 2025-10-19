#!/usr/bin/env python3
"""
OTA Update Trigger Script

Send firmware update notifications to ESP32 devices via MQTT
"""

import paho.mqtt.client as mqtt
import json
import sys
import time
import hashlib
import os

# MQTT Configuration
MQTT_SERVER = "192.168.1.48"
MQTT_PORT = 1883
MQTT_USERNAME = "8CBFEA8EA40C"  # Update with your device's MAC
DEVICE_HOSTNAME = "sensor_8EA40C"  # Update with your device's hostname

# JWT Configuration (same as your ESP32)
JWT_SECRET = "JWTSecret"

def generate_jwt_for_mqtt():
    """Generate JWT token for MQTT authentication"""
    try:
        import jwt
        
        current_time = int(time.time())
        expiration_time = current_time + 3600  # 1 hour
        
        payload = {
            "iss": "esp32-sensor",
            "sub": DEVICE_HOSTNAME,
            "aud": "emqx", 
            "exp": expiration_time,
            "iat": current_time,
            "device_id": "esp32s3_sensor_01",
            "mac_address": "8C:BF:EA:8E:A4:0C"
        }
        
        token = jwt.encode(payload, JWT_SECRET, algorithm="HS256")
        return token
    except ImportError:
        print("PyJWT not installed. Install with: pip install PyJWT")
        return None

def calculate_file_hash(file_path):
    """Calculate SHA256 hash of firmware file"""
    if not os.path.exists(file_path):
        return None
        
    sha256_hash = hashlib.sha256()
    with open(file_path, "rb") as f:
        for chunk in iter(lambda: f.read(4096), b""):
            sha256_hash.update(chunk)
    
    return sha256_hash.hexdigest()

def send_ota_update(firmware_version, firmware_url, firmware_file=None, force_update=False):
    """Send OTA update message to ESP32"""
    
    # Calculate signature if firmware file is provided
    signature = ""
    if firmware_file and os.path.exists(firmware_file):
        signature = calculate_file_hash(firmware_file)
        print(f"Calculated firmware hash: {signature}")
    else:
        signature = input("Enter firmware SHA256 hash: ").strip()
    
    if not signature:
        print("Error: Firmware signature is required")
        return False
    
    # Create update message
    update_message = {
        "version": firmware_version,
        "firmware_url": firmware_url,
        "signature": signature,
        "force_update": force_update,
        "timestamp": int(time.time()),
        "description": f"Firmware update to version {firmware_version}",
        "release_notes": "See changelog for details"
    }
    
    print("\n" + "="*60)
    print("OTA UPDATE MESSAGE")
    print("="*60)
    print(json.dumps(update_message, indent=2))
    print("="*60)
    
    # Generate JWT token
    jwt_token = generate_jwt_for_mqtt()
    if not jwt_token:
        return False
    
    # Connect to MQTT and send message
    client = mqtt.Client(client_id="ota_updater")
    client.username_pw_set(MQTT_USERNAME, jwt_token)
    
    connected = False
    connection_result = None
    
    def on_connect(client, userdata, flags, rc):
        nonlocal connected, connection_result
        connected = True
        connection_result = rc
        if rc == 0:
            print(f"✅ Connected to MQTT broker")
        else:
            print(f"❌ Failed to connect to MQTT broker: {rc}")
    
    def on_publish(client, userdata, mid):
        print(f"✅ Update message sent (message ID: {mid})")
    
    client.on_connect = on_connect
    client.on_publish = on_publish
    
    try:
        print(f"\n🔗 Connecting to MQTT broker {MQTT_SERVER}:{MQTT_PORT}")
        client.connect(MQTT_SERVER, MQTT_PORT, 60)
        client.loop_start()
        
        # Wait for connection
        timeout = 10
        while not connected and timeout > 0:
            time.sleep(0.1)
            timeout -= 0.1
        
        if connection_result != 0:
            print(f"❌ MQTT connection failed with code: {connection_result}")
            return False
        
        # Send update message
        update_topic = f"{DEVICE_HOSTNAME}/firmware/update"
        message_json = json.dumps(update_message)
        
        print(f"📤 Sending update to topic: {update_topic}")
        
        result = client.publish(update_topic, message_json)
        
        if result.rc == mqtt.MQTT_ERR_SUCCESS:
            print("✅ Update message queued successfully")
            time.sleep(2)  # Wait for message to be sent
            return True
        else:
            print(f"❌ Failed to send update message: {result.rc}")
            return False
        
    except Exception as e:
        print(f"❌ Error: {e}")
        return False
    
    finally:
        client.loop_stop()
        client.disconnect()

def main():
    """Main function"""
    print("🚀 ESP32 OTA Update Trigger")
    print("=" * 40)
    
    if len(sys.argv) < 3:
        print("Usage: python3 send_ota_update.py <version> <firmware_url> [firmware_file] [--force]")
        print("\nExamples:")
        print("  python3 send_ota_update.py 1.3.0 http://localhost:8080/firmware/esp32_v1.3.0.bin")
        print("  python3 send_ota_update.py 1.3.0 https://myserver.com/firmware.bin firmware.bin --force")
        sys.exit(1)
    
    firmware_version = sys.argv[1]
    firmware_url = sys.argv[2]
    firmware_file = sys.argv[3] if len(sys.argv) > 3 and not sys.argv[3].startswith('--') else None
    force_update = '--force' in sys.argv
    
    print(f"Firmware Version: {firmware_version}")
    print(f"Firmware URL: {firmware_url}")
    print(f"Force Update: {force_update}")
    print(f"Target Device: {DEVICE_HOSTNAME}")
    
    if firmware_file:
        print(f"Local Firmware File: {firmware_file}")
    
    # Confirm before sending
    confirm = input("\nSend OTA update to device? (y/N): ").strip().lower()
    if confirm != 'y':
        print("Update cancelled by user")
        sys.exit(0)
    
    # Send the update
    success = send_ota_update(firmware_version, firmware_url, firmware_file, force_update)
    
    if success:
        print("\n✅ OTA update message sent successfully!")
        print("Monitor the ESP32 serial output to see the update progress.")
        print("The device will automatically reboot after successful update.")
    else:
        print("\n❌ Failed to send OTA update message")
        sys.exit(1)

if __name__ == "__main__":
    main()
