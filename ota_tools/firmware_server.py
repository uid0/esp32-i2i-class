#!/usr/bin/env python3
"""
Simple Firmware Server for ESP32 OTA Updates

A basic HTTP server that serves firmware files with proper headers
for OTA updates. In production, use a proper web server like nginx.
"""

import http.server
import socketserver
import os
import json
import hashlib
from urllib.parse import urlparse, parse_qs
import threading
import time

class FirmwareHandler(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory="firmware", **kwargs)
    
    def do_GET(self):
        """Handle GET requests"""
        parsed_path = urlparse(self.path)
        
        if parsed_path.path == "/api/version":
            self.handle_version_check()
        elif parsed_path.path.startswith("/firmware/"):
            self.handle_firmware_download()
        else:
            super().do_GET()
    
    def handle_version_check(self):
        """Handle version check API endpoint"""
        try:
            # Read current firmware info
            with open("firmware/current_version.json", "r") as f:
                version_info = json.load(f)
            
            self.send_response(200)
            self.send_header('Content-Type', 'application/json')
            self.send_header('Access-Control-Allow-Origin', '*')
            self.end_headers()
            
            self.wfile.write(json.dumps(version_info, indent=2).encode())
            
        except FileNotFoundError:
            self.send_error(404, "Version info not found")
        except Exception as e:
            self.send_error(500, f"Server error: {e}")
    
    def handle_firmware_download(self):
        """Handle firmware file downloads with proper headers"""
        # Log the download request
        print(f"Firmware download requested: {self.path}")
        print(f"Client IP: {self.client_address[0]}")
        print(f"User-Agent: {self.headers.get('User-Agent', 'Unknown')}")
        
        # Set appropriate headers for firmware downloads
        if self.path.endswith('.bin'):
            self.send_response(200)
            self.send_header('Content-Type', 'application/octet-stream')
            self.send_header('Content-Disposition', f'attachment; filename="{os.path.basename(self.path)}"')
            self.send_header('Cache-Control', 'no-cache')
            
            # Add firmware size header
            firmware_path = "firmware" + self.path.replace("/firmware", "")
            if os.path.exists(firmware_path):
                file_size = os.path.getsize(firmware_path)
                self.send_header('Content-Length', str(file_size))
                print(f"Serving firmware: {firmware_path} ({file_size:,} bytes)")
            
            self.end_headers()
            
            # Serve the file
            try:
                with open(firmware_path, 'rb') as f:
                    self.wfile.write(f.read())
            except FileNotFoundError:
                self.send_error(404, "Firmware file not found")
        else:
            super().do_GET()

def create_sample_firmware_info():
    """Create sample firmware metadata"""
    firmware_dir = "firmware"
    os.makedirs(firmware_dir, exist_ok=True)
    
    # Create sample version info
    version_info = {
        "current_version": "1.3.0",
        "release_date": "2025-10-18",
        "description": "OTA update capability added",
        "firmware_url": "http://localhost:8080/firmware/esp32s3_firmware_v1.3.0.bin",
        "signature": "placeholder_hash_will_be_calculated",
        "size_bytes": 0,
        "release_notes": [
            "Added Over-The-Air update functionality",
            "Enhanced firmware versioning system", 
            "Improved MQTT connection stability",
            "Added cryptographic signature validation"
        ],
        "compatibility": {
            "min_version": "1.0.0",
            "board": "esp32-s3-devkitc-1"
        }
    }
    
    # Save version info
    with open(os.path.join(firmware_dir, "current_version.json"), "w") as f:
        json.dump(version_info, f, indent=2)
    
    print(f"Created sample firmware metadata in {firmware_dir}/")
    print("Place your .bin files in the firmware/ directory")

def main():
    """Start the firmware server"""
    PORT = 8080
    
    print("🚀 ESP32 Firmware Server")
    print("=" * 40)
    
    # Create firmware directory and sample files
    create_sample_firmware_info()
    
    print(f"Starting server on port {PORT}...")
    print(f"Firmware files directory: ./firmware/")
    print(f"Version check API: http://localhost:{PORT}/api/version")
    print(f"Firmware downloads: http://localhost:{PORT}/firmware/")
    print("\nPress Ctrl+C to stop the server")
    print("=" * 40)
    
    try:
        with socketserver.TCPServer(("", PORT), FirmwareHandler) as httpd:
            print(f"✅ Server running at http://localhost:{PORT}")
            print("\nTo test firmware updates:")
            print("1. Copy your .bin file to ./firmware/")
            print("2. Update current_version.json with correct info")
            print("3. Send MQTT update message to ESP32")
            print("\nServer log:")
            print("-" * 40)
            
            httpd.serve_forever()
            
    except KeyboardInterrupt:
        print("\n\n🛑 Server stopped by user")
    except Exception as e:
        print(f"\n❌ Server error: {e}")

if __name__ == "__main__":
    main()
