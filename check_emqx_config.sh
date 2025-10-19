#!/bin/bash

echo "=== EMQX Configuration Check ==="
echo "Server: 192.168.1.48:18083"
echo ""

# Check EMQX status
echo "1. EMQX Status:"
curl -s -u admin:public "http://192.168.1.48:18083/api/v5/status" | jq '.' 2>/dev/null || curl -s -u admin:public "http://192.168.1.48:18083/api/v5/status"
echo ""

# List authentication methods
echo "2. Authentication Methods:"
curl -s -u admin:public "http://192.168.1.48:18083/api/v5/authentication" | jq '.' 2>/dev/null || curl -s -u admin:public "http://192.168.1.48:18083/api/v5/authentication"
echo ""

# Check listeners
echo "3. MQTT Listeners:"
curl -s -u admin:public "http://192.168.1.48:18083/api/v5/listeners" | jq '.' 2>/dev/null || curl -s -u admin:public "http://192.168.1.48:18083/api/v5/listeners"
echo ""

# Test MQTT connectivity
echo "4. MQTT Port Test:"
timeout 5 telnet 192.168.1.48 1883 2>/dev/null && echo "MQTT port 1883 is accessible" || echo "MQTT port 1883 is NOT accessible"
echo ""

echo "=== Manual JWT Test ==="
echo "To test JWT authentication manually, you can use:"
echo "mosquitto_pub -h 192.168.1.48 -p 1883 -u '8CBFEA8EA40C' -P 'YOUR_JWT_TOKEN' -t 'test/topic' -m 'Hello'"
