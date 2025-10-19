#!/usr/bin/env python3
import base64
import json
import sys
from datetime import datetime

def decode_jwt_payload(token):
    try:
        # Split JWT token
        parts = token.split('.')
        if len(parts) != 3:
            print("❌ Invalid JWT format")
            return None
            
        # Decode payload (add padding if needed)
        payload = parts[1]
        # Add padding if needed
        missing_padding = len(payload) % 4
        if missing_padding:
            payload += '=' * (4 - missing_padding)
            
        # Decode base64
        decoded_bytes = base64.urlsafe_b64decode(payload)
        payload_json = json.loads(decoded_bytes.decode('utf-8'))
        
        return payload_json
        
    except Exception as e:
        print(f"❌ Error decoding JWT: {e}")
        return None

def check_jwt_validity(payload):
    if not payload:
        return False
        
    print("✅ JWT Token decoded successfully")
    print("📋 JWT Payload:")
    print(json.dumps(payload, indent=2))
    
    # Check expiration
    exp = payload.get('exp', 0)
    current_time = datetime.now().timestamp()
    
    if exp > current_time:
        print(f"✅ Token expires in {exp - current_time:.0f} seconds")
        return True
    else:
        print(f"❌ Token expired {current_time - exp:.0f} seconds ago")
        return False

if __name__ == "__main__":
    if len(sys.argv) > 1:
        token = sys.argv[1]
    else:
        print("=== JWT Token Decoder ===")
        print("Paste your JWT token:")
        token = input().strip()
    
    if token:
        payload = decode_jwt_payload(token)
        check_jwt_validity(payload)
    else:
        print("No token provided")
