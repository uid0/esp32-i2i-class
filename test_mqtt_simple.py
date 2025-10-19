#!/usr/bin/env python3
import socket
import struct

def test_mqtt_connection():
    try:
        # Create socket
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(5)
        
        # Connect to MQTT broker
        sock.connect(('192.168.1.48', 1883))
        print("✅ TCP connection to MQTT broker successful")
        
        # Send MQTT CONNECT packet
        client_id = "test_client"
        username = "8CBFEA8EA40C"
        password = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJlc3AzMi1kZXZpY2UiLCJzdWIiOiI4Q0JGRUE4RUE0MEMiLCJhdWQiOiJlbXF4IiwiZXhwIjoxNzYwNzc4NTIwLCJpYXQiOjE3NjA3NzQ5MjAsImRldmljZV9pZCI6ImVzcDMyczNfc2Vuc29yXzAxIiwibWFjX2FkZHJlc3MiOiI4QzpCRjpFQTo4RTpBNDowQyJ9.Q4ljOjUNalx00m6KsYmI5r-D31HJ3EK4QWYtARpjVAs"
        
        # MQTT CONNECT packet
        connect_packet = bytearray()
        
        # Fixed header
        connect_packet.append(0x10)  # CONNECT packet type
        
        # Variable header
        connect_packet.extend([0x00, 0x04])  # Protocol name length
        connect_packet.extend(b'MQTT')  # Protocol name
        connect_packet.append(0x04)  # Protocol level
        connect_packet.append(0xC2)  # Connect flags (username, password, clean session)
        connect_packet.extend([0x00, 0x3C])  # Keep alive
        
        # Payload
        # Client ID
        connect_packet.extend([0x00, len(client_id)])
        connect_packet.extend(client_id.encode())
        
        # Username
        connect_packet.extend([0x00, len(username)])
        connect_packet.extend(username.encode())
        
        # Password
        connect_packet.extend([0x00, len(password)])
        connect_packet.extend(password.encode())
        
        # Calculate remaining length and insert it
        remaining_length = len(connect_packet) - 1
        if remaining_length > 127:
            # Handle multi-byte remaining length encoding
            remaining_length_bytes = []
            while remaining_length > 0:
                byte = remaining_length % 128
                remaining_length = remaining_length // 128
                if remaining_length > 0:
                    byte |= 0x80
                remaining_length_bytes.append(byte)
            packet_with_length = bytearray([0x10]) + bytearray(remaining_length_bytes) + connect_packet[1:]
        else:
            packet_with_length = bytearray([0x10, remaining_length]) + connect_packet[1:]
        
        # Send packet
        sock.send(packet_with_length)
        print("✅ MQTT CONNECT packet sent")
        
        # Receive response
        response = sock.recv(1024)
        print(f"📥 Response received: {len(response)} bytes")
        
        if len(response) >= 4:
            if response[0] == 0x20:  # CONNACK
                if response[3] == 0x00:
                    print("✅ MQTT connection successful!")
                else:
                    print(f"❌ MQTT connection failed with code: {response[3]}")
                    print("Connection codes:")
                    print("  0x00: Connection accepted")
                    print("  0x01: Unacceptable protocol version")
                    print("  0x02: Identifier rejected")
                    print("  0x03: Server unavailable")
                    print("  0x04: Bad username or password")
                    print("  0x05: Not authorized")
            else:
                print(f"❌ Unexpected response type: {response[0]:02x}")
        
        sock.close()
        
    except Exception as e:
        print(f"❌ Error: {e}")

if __name__ == "__main__":
    test_mqtt_connection()
