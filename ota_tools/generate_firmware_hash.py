#!/usr/bin/env python3
"""
Firmware Hash Generator for ESP32 OTA Updates

This script generates SHA256 hashes for firmware files to be used
in the OTA update system.
"""

import hashlib
import sys
import os

def calculate_sha256(file_path):
    """Calculate SHA256 hash of a file"""
    sha256_hash = hashlib.sha256()
    
    try:
        with open(file_path, "rb") as f:
            # Read file in chunks to handle large files
            for chunk in iter(lambda: f.read(4096), b""):
                sha256_hash.update(chunk)
        
        return sha256_hash.hexdigest()
    
    except FileNotFoundError:
        print(f"Error: File '{file_path}' not found")
        return None
    except Exception as e:
        print(f"Error calculating hash: {e}")
        return None

def main():
    if len(sys.argv) != 2:
        print("Usage: python3 generate_firmware_hash.py <firmware.bin>")
        print("\nExample:")
        print("  python3 generate_firmware_hash.py firmware.bin")
        sys.exit(1)
    
    firmware_file = sys.argv[1]
    
    if not os.path.exists(firmware_file):
        print(f"Error: Firmware file '{firmware_file}' does not exist")
        sys.exit(1)
    
    print(f"Calculating SHA256 hash for: {firmware_file}")
    
    # Get file size
    file_size = os.path.getsize(firmware_file)
    print(f"File size: {file_size:,} bytes ({file_size/1024:.1f} KB)")
    
    # Calculate hash
    hash_value = calculate_sha256(firmware_file)
    
    if hash_value:
        print(f"\nSHA256 Hash: {hash_value}")
        
        # Save to file
        hash_file = firmware_file + ".sha256"
        with open(hash_file, 'w') as f:
            f.write(hash_value)
        
        print(f"Hash saved to: {hash_file}")
        
        # Generate MQTT update message example
        firmware_name = os.path.basename(firmware_file)
        mqtt_message = f'''{{
  "version": "1.3.0",
  "firmware_url": "https://your-server.com/firmware/{firmware_name}",
  "signature": "{hash_value}",
  "force_update": false,
  "description": "OTA update with new features",
  "release_notes": "Added Over-The-Air update capability"
}}'''
        
        print("\n" + "="*60)
        print("MQTT UPDATE MESSAGE EXAMPLE:")
        print("="*60)
        print(f"Topic: sensor_XXXXXX/firmware/update")
        print(f"Payload:")
        print(mqtt_message)
        print("="*60)
        
    else:
        sys.exit(1)

if __name__ == "__main__":
    main()
