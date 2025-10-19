#!/usr/bin/env python3
import paho.mqtt.client as mqtt
import jwt
import json
import time
from datetime import datetime

# Configuration
MQTT_SERVER = "192.168.1.48"
MQTT_PORT = 1883
JWT_SECRET = "JWTSecret"
MAC_ADDRESS = "8CBFEA8EA40C"  # Use the actual MAC from ESP32 logs
DEVICE_HOSTNAME = "sensor_8EA40C"  # Updated to match ESP32 hostname format

def generate_jwt():
    """Generate a JWT token matching the ESP32 format"""
    current_time = int(time.time())
    expiration_time = current_time + 3600  # 1 hour
    
    payload = {
        "iss": "esp32-sensor",  # Fixed to match EMQX config
        "sub": DEVICE_HOSTNAME,
        "aud": "emqx",
        "exp": expiration_time,
        "iat": current_time,
        "device_id": "esp32s3_sensor_01",
        "mac_address": f"{MAC_ADDRESS[:2]}:{MAC_ADDRESS[2:4]}:{MAC_ADDRESS[4:6]}:{MAC_ADDRESS[6:8]}:{MAC_ADDRESS[8:10]}:{MAC_ADDRESS[10:12]}"
    }
    
    token = jwt.encode(payload, JWT_SECRET, algorithm="HS256")
    print(f"Generated JWT: {token}")
    print(f"Payload: {json.dumps(payload, indent=2)}")
    return token

def on_connect(client, userdata, flags, rc):
    """Callback for when the client receives a CONNACK response from the server"""
    if rc == 0:
        print("✅ Connected to MQTT broker successfully!")
        
        # Subscribe to light control topic
        light_topic = f"{DEVICE_HOSTNAME}/light/control"
        client.subscribe(light_topic)
        print(f"📝 Subscribed to: {light_topic}")
        
        # Publish a test temperature reading
        temp_topic = f"{DEVICE_HOSTNAME}/sensors/temperature"
        test_payload = {
            "device_id": "esp32s3_sensor_01",
            "temperature": 22.5,
            "timestamp": int(time.time()),
            "unit": "celsius"
        }
        
        client.publish(temp_topic, json.dumps(test_payload))
        print(f"📤 Published test temperature to: {temp_topic}")
        
    else:
        print(f"❌ Failed to connect, return code {rc}")
        print("Connection result codes:")
        print("0: Connection successful")
        print("1: Connection refused - incorrect protocol version")
        print("2: Connection refused - invalid client identifier")
        print("3: Connection refused - server unavailable")
        print("4: Connection refused - bad username or password")
        print("5: Connection refused - not authorized")

def on_message(client, userdata, msg):
    """Callback for when a PUBLISH message is received from the server"""
    print(f"📥 Message received:")
    print(f"   Topic: {msg.topic}")
    print(f"   Payload: {msg.payload.decode()}")

def on_disconnect(client, userdata, rc):
    """Callback for when the client disconnects from the broker"""
    if rc != 0:
        print(f"❌ Unexpected disconnection! Code: {rc}")
    else:
        print("✅ Disconnected cleanly")

def test_mqtt_connection():
    """Test MQTT connection with JWT authentication"""
    print("=== MQTT Connection Test with JWT ===")
    
    # Generate JWT token
    jwt_token = generate_jwt()
    
    # Create MQTT client
    client = mqtt.Client(client_id=DEVICE_HOSTNAME)
    
    # Set callbacks
    client.on_connect = on_connect
    client.on_message = on_message
    client.on_disconnect = on_disconnect
    
    # Set username and password (JWT)
    client.username_pw_set(MAC_ADDRESS, jwt_token)
    
    # Set keep alive and other options
    client.keepalive = 60
    
    try:
        print(f"🔗 Connecting to {MQTT_SERVER}:{MQTT_PORT}")
        print(f"👤 Username: {MAC_ADDRESS}")
        print(f"🔐 Password (JWT): {jwt_token[:50]}...")
        
        # Connect to broker
        client.connect(MQTT_SERVER, MQTT_PORT, 60)
        
        # Start the loop to process network traffic and dispatch callbacks
        client.loop_start()
        
        # Keep connection alive for testing
        time.sleep(10)
        
        # Publish another test message
        status_topic = f"{DEVICE_HOSTNAME}/status"
        status_payload = {
            "device_id": "esp32s3_sensor_01",
            "status": "online",
            "timestamp": int(time.time()),
            "ip_address": "192.168.1.100",  # Example IP
            "mac_address": f"{MAC_ADDRESS[:2]}:{MAC_ADDRESS[2:4]}:{MAC_ADDRESS[4:6]}:{MAC_ADDRESS[6:8]}:{MAC_ADDRESS[8:10]}:{MAC_ADDRESS[10:12]}"
        }
        
        client.publish(status_topic, json.dumps(status_payload))
        print(f"📤 Published status to: {status_topic}")
        
        # Wait a bit more
        time.sleep(5)
        
        # Disconnect
        client.loop_stop()
        client.disconnect()
        
    except Exception as e:
        print(f"❌ Error: {e}")

if __name__ == "__main__":
    test_mqtt_connection()
