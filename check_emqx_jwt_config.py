#!/usr/bin/env python3
"""
Script to test different JWT configurations with EMQX
"""
import paho.mqtt.client as mqtt
import jwt
import json
import time
from datetime import datetime

# Configuration
MQTT_SERVER = "192.168.1.48"
MQTT_PORT = 1883
JWT_SECRET = "JWTSecret"
MAC_ADDRESS = "8CBFEA8EA40C"
DEVICE_HOSTNAME = "sensor_8EA40C"

def generate_jwt_variant(iss=None, aud=None, sub=None):
    """Generate JWT with different claim combinations"""
    current_time = int(time.time())
    expiration_time = current_time + 3600  # 1 hour
    
    payload = {
        "exp": expiration_time,
        "iat": current_time,
        "device_id": "esp32s3_sensor_01",
        "mac_address": "8C:BF:EA:8E:A4:0C"
    }
    
    if iss:
        payload["iss"] = iss
    if aud:
        payload["aud"] = aud  
    if sub:
        payload["sub"] = sub
    
    token = jwt.encode(payload, JWT_SECRET, algorithm="HS256")
    return token, payload

def test_mqtt_connection_variant(token, description, payload):
    """Test MQTT connection with a specific JWT variant"""
    print(f"\n=== Testing: {description} ===")
    print(f"JWT Payload: {json.dumps(payload, indent=2)}")
    
    client = mqtt.Client(client_id=DEVICE_HOSTNAME)
    client.username_pw_set(MAC_ADDRESS, token)
    
    connected = False
    connection_result = None
    
    def on_connect(client, userdata, flags, rc):
        nonlocal connected, connection_result
        connected = True
        connection_result = rc
        if rc == 0:
            print(f"✅ SUCCESS: Connected with {description}")
            client.disconnect()
        else:
            print(f"❌ FAILED: Return code {rc} for {description}")
    
    client.on_connect = on_connect
    
    try:
        client.connect(MQTT_SERVER, MQTT_PORT, 10)
        client.loop_start()
        
        # Wait for connection result
        timeout = 10
        while not connected and timeout > 0:
            time.sleep(0.1)
            timeout -= 0.1
            
        client.loop_stop()
        
        if not connected:
            print(f"❌ TIMEOUT: No response for {description}")
            return False
            
        return connection_result == 0
        
    except Exception as e:
        print(f"❌ ERROR: {e} for {description}")
        return False

def main():
    """Test various JWT configurations"""
    print("🔍 Testing different JWT configurations with EMQX...")
    
    # Test different combinations
    test_cases = [
        # Original configuration
        ("esp32-sensor", "emqx", DEVICE_HOSTNAME, "Original: iss=esp32-sensor, aud=emqx"),
        
        # Try without audience
        ("esp32-sensor", None, DEVICE_HOSTNAME, "No audience: iss=esp32-sensor only"),
        
        # Try different audience values
        ("esp32-sensor", "mqtt", DEVICE_HOSTNAME, "Different aud: iss=esp32-sensor, aud=mqtt"),
        ("esp32-sensor", "broker", DEVICE_HOSTNAME, "Different aud: iss=esp32-sensor, aud=broker"),
        ("esp32-sensor", "", DEVICE_HOSTNAME, "Empty aud: iss=esp32-sensor, aud=''"),
        
        # Try different issuer values
        ("esp32", "emqx", DEVICE_HOSTNAME, "Different iss: iss=esp32, aud=emqx"),
        ("device", "emqx", DEVICE_HOSTNAME, "Different iss: iss=device, aud=emqx"),
        ("sensor", "emqx", DEVICE_HOSTNAME, "Different iss: iss=sensor, aud=emqx"),
        
        # Try without issuer
        (None, "emqx", DEVICE_HOSTNAME, "No issuer: aud=emqx only"),
        
        # Try minimal JWT (just required claims)
        (None, None, None, "Minimal JWT: only exp, iat, device_id"),
        
        # Try with MAC as subject
        ("esp32-sensor", "emqx", MAC_ADDRESS, "MAC as subject: sub=MAC_ADDRESS"),
    ]
    
    successful_configs = []
    
    for iss, aud, sub, description in test_cases:
        token, payload = generate_jwt_variant(iss, aud, sub)
        if test_mqtt_connection_variant(token, description, payload):
            successful_configs.append((iss, aud, sub, description))
    
    print(f"\n{'='*60}")
    print("🎯 RESULTS SUMMARY")
    print(f"{'='*60}")
    
    if successful_configs:
        print("✅ SUCCESSFUL CONFIGURATIONS:")
        for iss, aud, sub, desc in successful_configs:
            print(f"   - {desc}")
            print(f"     iss: {iss}, aud: {aud}, sub: {sub}")
    else:
        print("❌ NO SUCCESSFUL CONFIGURATIONS FOUND")
        print("\nThis suggests either:")
        print("1. EMQX JWT authentication is not enabled")
        print("2. JWT secret mismatch")
        print("3. EMQX is expecting different claim structure")
        print("4. Network/firewall issues")
        
    print(f"\n{'='*60}")

if __name__ == "__main__":
    main()
