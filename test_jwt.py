#!/usr/bin/env python3
import jwt
import json
from datetime import datetime

# Your JWT secret
JWT_SECRET = "JWTSecret"

def test_jwt_token(token):
    try:
        # Decode the JWT token
        decoded = jwt.decode(token, JWT_SECRET, algorithms=["HS256"])
        
        print("✅ JWT Token is VALID")
        print("📋 Decoded payload:")
        print(json.dumps(decoded, indent=2))
        
        # Check expiration
        exp = decoded.get('exp', 0)
        current_time = datetime.now().timestamp()
        
        if exp > current_time:
            print(f"✅ Token expires in {exp - current_time:.0f} seconds")
        else:
            print(f"❌ Token expired {current_time - exp:.0f} seconds ago")
            
        return True
        
    except jwt.ExpiredSignatureError:
        print("❌ JWT Token EXPIRED")
        return False
    except jwt.InvalidTokenError as e:
        print(f"❌ JWT Token INVALID: {e}")
        return False

if __name__ == "__main__":
    print("=== JWT Token Validator ===")
    print("Paste your JWT token from the ESP32 serial output:")
    token = input().strip()
    
    if token:
        test_jwt_token(token)
    else:
        print("No token provided")
