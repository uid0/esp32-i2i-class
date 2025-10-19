# MQTT Disconnection Troubleshooting Guide

## Issue Summary
Your ESP32-S3 is getting disconnected from the MQTT server. Based on testing, the primary issue appears to be **JWT authentication failure** (MQTT return code 4: "Bad username or password").

## Root Cause Analysis

### ✅ What's Working
1. **TCP Connectivity**: Successfully connects to `192.168.1.48:1883`
2. **WiFi Connection**: ESP32 connects to WiFi properly
3. **JWT Generation**: Code generates valid JWT tokens with correct structure
4. **Code Improvements**: Fixed JWT issuer from `esp32-device` to `esp32-sensor`

### ❌ What's Not Working
1. **MQTT Authentication**: Return code 4 (bad credentials) when connecting
2. **EMQX Configuration**: JWT authentication may not be properly configured

## Test Results

### Python MQTT Test
```bash
python3 test_mqtt_paho.py
```
**Result**: Connection refused with return code 4 (bad username/password)

**Generated JWT Payload**:
```json
{
  "iss": "esp32-sensor",
  "sub": "sensor_a40c", 
  "aud": "emqx",
  "exp": 1760779421,
  "iat": 1760775821,
  "device_id": "esp32s3_sensor_01",
  "mac_address": "8C:BF:EA:8E:A4:0C"
}
```

## Immediate Actions Required

### 1. **CRITICAL: Configure EMQX JWT Authentication**

Access EMQX Dashboard at `http://192.168.1.48:18083`:
- Username: `admin`  
- Password: `4ian1234`

#### Option A: Web Dashboard
1. Go to **Authentication** → **Authentication**
2. Click **Create** button
3. Select **JWT** authentication method
4. Configure:
   ```
   Authentication Name: jwt_auth
   JWT Secret: JWTSecret
   JWT From: password
   Verify Claims: true
   ```
5. Add Claims:
   - `iss`: `esp32-sensor`
   - `aud`: `emqx`
   - `sub`: (leave empty or match username)

#### Option B: HTTP API (if API access works)
```bash
curl -X POST "http://192.168.1.48:18083/api/v5/authentication" \
-H "Content-Type: application/json" \
-u admin:4ian1234 \
-d '{
  "mechanism": "jwt",
  "backend": "built_in_database", 
  "jwt_secret": "JWTSecret",
  "jwt_from": "password",
  "verify_claims": true,
  "claims": {
    "iss": "esp32-sensor",
    "aud": "emqx"
  }
}'
```

### 2. **Verify JWT Configuration**

Check if JWT authentication is enabled:
```bash
curl -u admin:4ian1234 "http://192.168.1.48:18083/api/v5/authentication"
```

### 3. **Test After Configuration**

Run the test again:
```bash
python3 test_mqtt_paho.py
```

Expected result: Connection successful (return code 0)

## Code Improvements Made

### ✅ Fixed Issues
1. **JWT Issuer**: Changed from `esp32-device` to `esp32-sensor`
2. **MQTT Timeouts**: Added 60s keepalive and 30s socket timeout
3. **Enhanced Debugging**: Added WiFi status, heap memory, and time sync status
4. **JWT Refresh**: Automatic token refresh every 30 minutes
5. **Connection Monitoring**: Better connection state management

### ESP32 Code Changes
- Added `setKeepAlive(60)` and `setSocketTimeout(30)`
- Enhanced error reporting with system status
- Automatic JWT token refresh before expiration
- More robust connection state tracking

## Verification Steps

1. **Upload Updated ESP32 Code**
   ```bash
   pio run --target upload --target monitor
   ```

2. **Monitor Serial Output**
   Look for:
   - WiFi connection success
   - Time synchronization
   - JWT token generation
   - MQTT connection attempts and results

3. **Expected Behavior After Fix**
   - ESP32 connects to WiFi
   - Generates valid JWT token
   - Successfully connects to EMQX
   - Return code 0 (connected)
   - Publishes temperature data
   - Maintains stable connection

## Additional Debugging

### EMQX Logs
Check EMQX logs for authentication failures:
```bash
# If you have SSH access to EMQX server
tail -f /var/log/emqx/emqx.log
```

### ESP32 Serial Monitor
Watch for these specific messages:
- `MQTT connection failed! State: 4` (bad credentials)
- `MQTT connection failed! State: 5` (unauthorized)
- `MQTT connected!` (success)

## Network Considerations

- **Firewall**: Ensure port 1883 is open
- **Network Stability**: Check for network interruptions
- **DNS Resolution**: IP address `192.168.1.48` is hardcoded (good)
- **WiFi Signal**: Ensure strong signal strength

## Security Notes

- JWT secret `JWTSecret` should be changed in production
- Consider using TLS/SSL (port 8883) for production
- Monitor EMQX authentication logs
- Implement proper network security

## Next Steps

1. **Configure EMQX JWT authentication** (CRITICAL)
2. Test Python MQTT client connection
3. Upload updated ESP32 code
4. Monitor for stable connection
5. Consider implementing TLS for production use

---

**Status**: Ready for EMQX configuration. The ESP32 code is improved and should connect successfully once EMQX JWT authentication is properly configured.
