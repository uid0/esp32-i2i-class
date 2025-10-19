#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <ArduinoJson.h>
#include <ArduinoJWT.h>
#include <time.h>
#include <HTTPClient.h>
#include <Update.h>
#include <esp_ota_ops.h>
#include <mbedtls/md.h>
#include <mbedtls/sha256.h>
#include <FastLED.h>

// Firmware Version
#define FIRMWARE_VERSION "1.6.0"
#define BUILD_TIMESTAMP __DATE__ " " __TIME__

// Configuration
const char* WIFI_SSID = "Legacy-Wifi";
const char* WIFI_PASSWORD = "Asp3nsH0use";
const char* MQTT_SERVER = "192.168.1.48";
const int MQTT_PORT = 1883;

// JWT Configuration
const char* JWT_SECRET = "JWTSecret";
const char* DEVICE_ID = "esp32s3_sensor_01";

// NTP Configuration
const char* NTP_SERVER = "pool.ntp.org";
const long GMT_OFFSET_SEC = -8 * 3600; // PST timezone (adjust as needed)
const int DAYLIGHT_OFFSET_SEC = 3600;

// Hardware pins for Seeed Studio XIAO-ESP32-S3
const int TEMP_SENSOR_PIN = D0;  // GPIO1 (D0 on XIAO)
const int LIGHT_PIN = D1;        // GPIO2 (D1 on XIAO) 
const int LED_PIN = LED_BUILTIN; // GPIO21 - User LED on XIAO-ESP32-S3
const int NEOPIXEL_PIN = A0;     // GPIO2 (A0 on XIAO) - Good for Neopixels
const int NUM_LEDS = 60;

// Timing
const unsigned long TEMP_READ_INTERVAL = 30000; // 30 seconds
const unsigned long JWT_REFRESH_INTERVAL = 1800000; // 30 minutes (refresh JWT before 1hr expiry)
const unsigned long OTA_CHECK_INTERVAL = 300000; // 5 minutes (check for updates)

// OTA Configuration
const char* OTA_SERVER_URL = "https://your-firmware-server.com"; // Update this
const char* OTA_PUBLIC_KEY = "-----BEGIN PUBLIC KEY-----\n"
                            "MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEA...\n"  // Update with your public key
                            "-----END PUBLIC KEY-----\n";

// Objects
OneWire oneWire(TEMP_SENSOR_PIN);
DallasTemperature sensors(&oneWire);
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

// Neopixel setup
CRGB leds[NUM_LEDS];
CRGB currentColor = CRGB::Black;
uint8_t brightness = 50; // Default brightness (0-255)

// Variables
unsigned long lastTempRead = 0;
unsigned long lastMQTTAttempt = 0;
unsigned long lastLEDUpdate = 0;
unsigned long lastJWTRefresh = 0;
unsigned long lastOTACheck = 0;
String deviceHostname;
String mqttUsername;
String mqttPassword;
bool timeSynced = false;
bool wifiConnected = false;
bool mqttConnected = false;
bool mqttConnecting = false;
bool otaInProgress = false;

// MQTT activity tracking
unsigned long lastMQTTSend = 0;
unsigned long lastMQTTReceive = 0;
const unsigned long MQTT_FLASH_DURATION = 150; // ms
const unsigned long MQTT_BLACKOUT_DURATION = 2000; // 2 seconds of black after activity
bool inMQTTBlackout = false; // Track if we're in post-activity blackout

// Global variables for breathing animation
unsigned long breathingLastUpdate = 0;
float breathingBrightness = 0.0;
float breathingDirection = 1.0;

// OTA Status
enum OTAStatus {
  OTA_IDLE,
  OTA_CHECKING,
  OTA_DOWNLOADING,
  OTA_VALIDATING,
  OTA_INSTALLING,
  OTA_SUCCESS,
  OTA_FAILED
};

OTAStatus currentOTAStatus = OTA_IDLE;

// LED Status
enum LEDStatus {
  LED_OFF,
  LED_WIFI_CONNECTING,    // Slow blink - Blue
  LED_MQTT_CONNECTING,    // Fast blink - Yellow  
  LED_MQTT_CONNECTED,     // Breathing fade - Dim Green
  LED_MQTT_SENDING,       // Brief flash - White
  LED_MQTT_RECEIVING,     // Brief flash - Red
  LED_OTA_DOWNLOADING,    // Progress bar - Orange
  LED_OTA_VALIDATING,     // Spinning - Cyan
  LED_OTA_INSTALLING,     // Fast pulse - Red
  LED_TEMPERATURE_READ,   // Brief flash - White
  LED_ERROR               // Red blink - Red
};

LEDStatus currentLEDStatus = LED_OFF;

// Function declarations
void connectToWiFi();
void syncTime();
void connectToMQTT();
void mqttCallback(char* topic, byte* payload, unsigned int length);
void readAndPublishTemperature();
void publishStatus(const char* status);
String generateJWT();
unsigned long getCurrentUnixTime();
void setLEDStatus(LEDStatus status);
void updateLEDStatus();
void checkConnections();

// Neopixel Function declarations
void initializeNeopixels();
void setNeopixelColor(CRGB color, uint8_t brightness = 50);
void setNeopixelProgress(float progress, CRGB color);
void neopixelSpinning(CRGB color, uint32_t speed = 100);
void neopixelPulse(CRGB color, uint32_t speed = 1000);
void neopixelBlink(CRGB color, uint32_t interval = 500);
void neopixelRainbow(uint8_t speed = 5);
void neopixelTemperatureFlash();
void neopixelBreathe(CRGB color, uint32_t speed = 3000);
void neopixelFlash(CRGB color, uint32_t duration = 100);
void resetBreathingAnimation();
void updateNeopixels();

// OTA Function declarations
void handleFirmwareUpdateMessage(JsonDocument& updateMsg);
bool isNewerVersion(const String& newVersion, const String& currentVersion);
void performOTAUpdate(const String& firmwareUrl, const String& version, const String& signature);
bool downloadFirmware(const String& url, size_t& downloadedSize);
bool validateFirmwareSignature(const String& signature, size_t firmwareSize);
void publishOTAStatus(OTAStatus status, const String& message = "");
String calculateFirmwareSHA256();
void checkForFirmwareUpdates();

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("=== Seeed XIAO-ESP32-S3 MQTT Sensor with JWT ===");
  Serial.print("Firmware Version: ");
  Serial.println(FIRMWARE_VERSION);
  Serial.print("Build Timestamp: ");
  Serial.println(BUILD_TIMESTAMP);
  Serial.println("Board: Seeed Studio XIAO-ESP32-S3");
  
  // Initialize hardware
  pinMode(LIGHT_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LIGHT_PIN, LOW);
  digitalWrite(LED_PIN, LOW);
  
  Serial.print("Built-in LED pin: ");
  Serial.println(LED_PIN);
  Serial.println("Testing built-in LED...");
  
  // Test built-in LED
  for (int i = 0; i < 3; i++) {
    digitalWrite(LED_PIN, HIGH);
    delay(200);
    digitalWrite(LED_PIN, LOW);
    delay(200);
  }
  
  // Initialize Neopixels
  initializeNeopixels();
  
  // Initialize temperature sensor
  sensors.begin();
  Serial.println("Temperature sensor initialized");
  
  // Generate device hostname from MAC
  String macAddress = WiFi.macAddress();
  macAddress.replace(":", "");
  deviceHostname = "sensor_" + macAddress.substring(6); // Use last 6 chars
  Serial.print("Device hostname: ");
  Serial.println(deviceHostname);
  
  // Generate MQTT credentials using MAC address as username
  mqttUsername = macAddress; // Use full MAC address as username
  
  // Connect to WiFi first
  connectToWiFi();
  
  // Give WiFi stack time to fully initialize
  if (wifiConnected) {
    Serial.println("Waiting for network stack to stabilize...");
    delay(2000);
  }
  
  // Sync time with NTP
  syncTime();
  
  // Generate JWT password after time sync
  mqttPassword = generateJWT();
  lastJWTRefresh = millis(); // Initialize JWT refresh timer
  
  Serial.print("MQTT Username (MAC): ");
  Serial.println(mqttUsername);
  Serial.print("MQTT Password (JWT): ");
  Serial.println(mqttPassword);
  
  Serial.println("Setup complete! MQTT connection will be attempted in the main loop.");
}

void loop() {
  // Update LED status
  updateLEDStatus();
  
  // Update Neopixels
  updateNeopixels();
  
  // Check and maintain connections
  checkConnections();
  
  // Handle MQTT messages
  if (mqttConnected) {
    mqttClient.loop();
  }
  
  // Read temperature periodically
  if (mqttConnected && millis() - lastTempRead > TEMP_READ_INTERVAL) {
    readAndPublishTemperature(); // LED flash handled in publish function
    lastTempRead = millis();
  }
  
  // Refresh JWT periodically (every 30 minutes)
  if (timeSynced && millis() - lastJWTRefresh > JWT_REFRESH_INTERVAL) {
    Serial.println("Refreshing JWT token...");
    mqttPassword = generateJWT();
    lastJWTRefresh = millis();
    
    // If currently connected, disconnect and reconnect with new token
    if (mqttConnected) {
      Serial.println("Reconnecting with fresh JWT token");
      mqttClient.disconnect();
      mqttConnected = false;
      mqttConnecting = false;
    }
  }
  
  // Check for firmware updates periodically (every 5 minutes)
  if (!otaInProgress && mqttConnected && millis() - lastOTACheck > OTA_CHECK_INTERVAL) {
    lastOTACheck = millis();
    checkForFirmwareUpdates();
  }
  
  delay(100);
}

void connectToWiFi() {
  Serial.println("=== WiFi Connection ===");
  Serial.print("SSID: ");
  Serial.println(WIFI_SSID);
  Serial.print("Password length: ");
  Serial.println(strlen(WIFI_PASSWORD));
  
  setLEDStatus(LED_WIFI_CONNECTING);
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    Serial.println();
    Serial.print("WiFi connected! IP: ");
    Serial.println(WiFi.localIP());
    Serial.print("MAC: ");
    Serial.println(WiFi.macAddress());
    Serial.print("Gateway: ");
    Serial.println(WiFi.gatewayIP());
    Serial.print("DNS: ");
    Serial.println(WiFi.dnsIP());
  } else {
    wifiConnected = false;
    Serial.println();
    Serial.println("WiFi connection failed!");
    Serial.print("Status: ");
    Serial.println(WiFi.status());
  }
}

void syncTime() {
  Serial.println("=== Time Synchronization ===");
  
  // Configure time with NTP
  configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);
  
  Serial.print("Waiting for time sync from ");
  Serial.print(NTP_SERVER);
  Serial.println("...");
  
  int attempts = 0;
  while (!timeSynced && attempts < 20) {
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
      timeSynced = true;
      Serial.println("Time synchronized!");
      
      // Print current time
      Serial.print("Current time: ");
      Serial.print(timeinfo.tm_year + 1900);
      Serial.print("-");
      Serial.print(timeinfo.tm_mon + 1);
      Serial.print("-");
      Serial.print(timeinfo.tm_mday);
      Serial.print(" ");
      Serial.print(timeinfo.tm_hour);
      Serial.print(":");
      Serial.print(timeinfo.tm_min);
      Serial.print(":");
      Serial.println(timeinfo.tm_sec);
      
      // Print Unix timestamp
      unsigned long unixTime = getCurrentUnixTime();
      Serial.print("Unix timestamp: ");
      Serial.println(unixTime);
    } else {
      Serial.print(".");
      delay(1000);
      attempts++;
    }
  }
  
  if (!timeSynced) {
    Serial.println();
    Serial.println("Time synchronization failed!");
    Serial.println("JWT tokens may be invalid due to incorrect timestamps");
  }
}

unsigned long getCurrentUnixTime() {
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    return mktime(&timeinfo);
  }
  return 0;
}

void connectToMQTT() {
  if (mqttConnecting) {
    return; // Already attempting to connect
  }
  
  Serial.println("=== MQTT Connection ===");
  Serial.print("Server: ");
  Serial.println(MQTT_SERVER);
  Serial.print("Port: ");
  Serial.println(MQTT_PORT);
  Serial.print("Username (MAC): ");
  Serial.println(mqttUsername);
  Serial.print("Password (JWT): ");
  Serial.println(mqttPassword);
  
  setLEDStatus(LED_MQTT_CONNECTING);
  mqttConnecting = true;
  
  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);
  
  // Set MQTT connection options for better reliability
  mqttClient.setKeepAlive(60);    // 60 second keepalive
  mqttClient.setSocketTimeout(30); // 30 second socket timeout
  mqttClient.setBufferSize(1024); // Increase buffer size for large JWT tokens
  
  // Create a unique client ID with version info to avoid conflicts and enable version tracking
  String clientId = deviceHostname + "_v" + FIRMWARE_VERSION + "_" + String(millis() % 10000);
  Serial.print("Client ID: ");
  Serial.println(clientId);
  
  if (mqttClient.connect(clientId.c_str(), mqttUsername.c_str(), mqttPassword.c_str())) {
    Serial.println("MQTT connected!");
    mqttConnected = true;
    mqttConnecting = false;
    setLEDStatus(LED_MQTT_CONNECTED);
    
    // Subscribe to light control topic
    String lightTopic = deviceHostname + "/light/control";
    mqttClient.subscribe(lightTopic.c_str());
    Serial.print("Subscribed to: ");
    Serial.println(lightTopic);
    
    // Subscribe to firmware update topic
    String firmwareUpdateTopic = deviceHostname + "/firmware/update";
    mqttClient.subscribe(firmwareUpdateTopic.c_str());
    Serial.print("Subscribed to firmware updates: ");
    Serial.println(firmwareUpdateTopic);
    
    // Publish device status
    publishStatus("online");
  } else {
    Serial.print("MQTT connection failed! State: ");
    Serial.println(mqttClient.state());
    Serial.println("MQTT State Codes:");
    Serial.println("-4: Connection timeout");
    Serial.println("-3: Connection lost");
    Serial.println("-2: Connect failed");
    Serial.println("-1: Disconnected");
    Serial.println("0: Connected");
    Serial.println("1: Bad protocol");
    Serial.println("2: Bad client ID");
    Serial.println("3: Unavailable");
    Serial.println("4: Bad credentials");
    Serial.println("5: Unauthorized");
    
    // Additional debugging info
    Serial.print("WiFi Status: ");
    Serial.println(WiFi.status());
    Serial.print("Free Heap: ");
    Serial.println(ESP.getFreeHeap());
    Serial.print("Time Synced: ");
    Serial.println(timeSynced ? "Yes" : "No");
    
    mqttConnected = false;
    mqttConnecting = false;
    
    // If authentication failed, regenerate JWT
    if (mqttClient.state() == 4 || mqttClient.state() == 5) {
      Serial.println("Authentication failed - regenerating JWT token");
      mqttPassword = generateJWT();
      Serial.print("New JWT token: ");
      Serial.println(mqttPassword);
    }
  }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.println("=== MQTT Message Received ===");
  Serial.print("Topic: ");
  Serial.println(topic);
  
  // Flash red for incoming message
  setLEDStatus(LED_MQTT_RECEIVING);
  lastMQTTReceive = millis();
  
  String message = "";
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  Serial.print("Payload: ");
  Serial.println(message);
  
  // Handle light control
  String lightTopic = deviceHostname + "/light/control";
  String firmwareUpdateTopic = deviceHostname + "/firmware/update";
  
  if (String(topic) == lightTopic) {
    if (message == "on") {
      digitalWrite(LIGHT_PIN, HIGH);
      Serial.println("Light turned ON");
    } else if (message == "off") {
      digitalWrite(LIGHT_PIN, LOW);
      Serial.println("Light turned OFF");
    }
    
    // Flash white for outgoing message
    setLEDStatus(LED_MQTT_SENDING);
    lastMQTTSend = millis();
    
    // Publish light status
    String statusTopic = deviceHostname + "/light/status";
    String status = digitalRead(LIGHT_PIN) ? "on" : "off";
    mqttClient.publish(statusTopic.c_str(), status.c_str());
  }
  
  // Handle firmware update messages
  if (String(topic) == firmwareUpdateTopic) {
    Serial.println("Received firmware update message");
    JsonDocument updateMsg;
    DeserializationError error = deserializeJson(updateMsg, message);
    
    if (!error) {
      handleFirmwareUpdateMessage(updateMsg);
    } else {
      Serial.print("Failed to parse firmware update message: ");
      Serial.println(error.c_str());
    }
  }
}

void readAndPublishTemperature() {
  Serial.println("=== Reading Temperature ===");
  
  // LED flash is handled in the publish function
  
  sensors.requestTemperatures();
  float temperature = sensors.getTempCByIndex(0);
  
  if (temperature != DEVICE_DISCONNECTED_C) {
    Serial.print("Temperature: ");
    Serial.print(temperature);
    Serial.println("°C");
    
    // Create JSON payload
    JsonDocument doc;
    doc["device_id"] = DEVICE_ID;
    doc["temperature"] = temperature;
    doc["timestamp"] = getCurrentUnixTime();
    doc["unit"] = "celsius";
    
    String jsonString;
    serializeJson(doc, jsonString);
    
    // Flash white for outgoing message
    setLEDStatus(LED_MQTT_SENDING);
    lastMQTTSend = millis();
    
    // Publish to MQTT
    String tempTopic = deviceHostname + "/sensors/temperature";
    mqttClient.publish(tempTopic.c_str(), jsonString.c_str());
    Serial.print("Published to: ");
    Serial.println(tempTopic);
  } else {
    Serial.println("Temperature sensor error!");
  }
}

void publishStatus(const char* status) {
  JsonDocument doc;
  doc["device_id"] = DEVICE_ID;
  doc["status"] = status;
  doc["timestamp"] = getCurrentUnixTime();
  doc["ip_address"] = WiFi.localIP().toString();
  doc["mac_address"] = WiFi.macAddress();
  doc["firmware_version"] = FIRMWARE_VERSION;
  doc["build_timestamp"] = BUILD_TIMESTAMP;
  doc["free_heap"] = ESP.getFreeHeap();
  
  String jsonString;
  serializeJson(doc, jsonString);
  
  // Flash white for outgoing message
  setLEDStatus(LED_MQTT_SENDING);
  lastMQTTSend = millis();
  
  String statusTopic = deviceHostname + "/status";
  mqttClient.publish(statusTopic.c_str(), jsonString.c_str());
  Serial.print("Status published: ");
  Serial.println(status);
}

String generateJWT() {
  Serial.println("=== Generating JWT ===");
  
  if (!timeSynced) {
    Serial.println("WARNING: Time not synchronized - JWT may be invalid!");
  }
  
  ArduinoJWT jwt(JWT_SECRET);
  
  // Get current Unix timestamp
  unsigned long currentTime = getCurrentUnixTime();
  unsigned long expirationTime = currentTime + 3600; // 1 hour from now
  
  Serial.print("Current Unix time: ");
  Serial.println(currentTime);
  Serial.print("Expiration time: ");
  Serial.println(expirationTime);
  
  // Create JSON payload as String
  JsonDocument payload;
  payload["iss"] = "esp32-sensor";  // Fixed: Changed from "esp32-device" to match EMQX config
  payload["sub"] = deviceHostname; // Use device hostname as subject
  payload["aud"] = "emqx";
  payload["exp"] = expirationTime;
  payload["iat"] = currentTime;
  payload["device_id"] = DEVICE_ID;
  payload["mac_address"] = WiFi.macAddress();
  payload["firmware_version"] = FIRMWARE_VERSION;
  payload["build_timestamp"] = BUILD_TIMESTAMP;
  
  String payloadString;
  serializeJson(payload, payloadString);
  
  Serial.print("JWT Payload: ");
  Serial.println(payloadString);
  
  String jwtToken = jwt.encodeJWT(payloadString);
  Serial.print("JWT generated: ");
  Serial.println(jwtToken);
  
  return jwtToken;
}

void setLEDStatus(LEDStatus status) {
  if (currentLEDStatus != status) {
    currentLEDStatus = status;
    Serial.print("LED Status changed to: ");
    switch (status) {
      case LED_OFF: Serial.println("OFF"); break;
      case LED_WIFI_CONNECTING: Serial.println("WiFi Connecting (blue blink)"); break;
      case LED_MQTT_CONNECTING: Serial.println("MQTT Connecting (yellow blink)"); break;
      case LED_MQTT_CONNECTED: Serial.println("MQTT Connected (green breathing)"); break;
      case LED_MQTT_SENDING: Serial.println("MQTT Sending (white flash)"); break;
      case LED_MQTT_RECEIVING: Serial.println("MQTT Receiving (red flash)"); break;
      case LED_OTA_DOWNLOADING: Serial.println("OTA Downloading (orange progress)"); break;
      case LED_OTA_VALIDATING: Serial.println("OTA Validating (cyan spin)"); break;
      case LED_OTA_INSTALLING: Serial.println("OTA Installing (red pulse)"); break;
      case LED_TEMPERATURE_READ: Serial.println("Temperature Reading (white flash)"); break;
      case LED_ERROR: Serial.println("Error (red blink)"); break;
    }
  }
}

void updateLEDStatus() {
  static unsigned long lastBlink = 0;
  static bool ledState = false;
  unsigned long now = millis();
  
  // Update built-in LED (simple on/off indication)
  switch (currentLEDStatus) {
    case LED_OFF:
      digitalWrite(LED_PIN, LOW);
      break;
      
    case LED_WIFI_CONNECTING:
      // Slow blink (1 second intervals)
      if (now - lastBlink > 1000) {
        ledState = !ledState;
        digitalWrite(LED_PIN, ledState);
        lastBlink = now;
      }
      break;
      
    case LED_MQTT_CONNECTING:
      // Fast blink (250ms intervals)
      if (now - lastBlink > 250) {
        ledState = !ledState;
        digitalWrite(LED_PIN, ledState);
        lastBlink = now;
      }
      break;
      
    case LED_MQTT_CONNECTED:
      // Solid on
      digitalWrite(LED_PIN, HIGH);
      break;
      
    case LED_MQTT_SENDING:
    case LED_MQTT_RECEIVING:
    case LED_TEMPERATURE_READ:
      // Brief solid on for activity
      digitalWrite(LED_PIN, HIGH);
      break;
      
    case LED_OTA_DOWNLOADING:
    case LED_OTA_VALIDATING:
    case LED_OTA_INSTALLING:
      // Fast blink during OTA
      if (now - lastBlink > 100) {
        ledState = !ledState;
        digitalWrite(LED_PIN, ledState);
        lastBlink = now;
      }
      break;
      
    case LED_ERROR:
      // Very fast blink for errors
      if (now - lastBlink > 50) {
        ledState = !ledState;
        digitalWrite(LED_PIN, ledState);
        lastBlink = now;
      }
      break;
  }
}

void checkConnections() {
  // Check WiFi connection
  if (WiFi.status() != WL_CONNECTED) {
    if (wifiConnected) {
      Serial.println("WiFi connection lost!");
      wifiConnected = false;
      mqttConnected = false;
      mqttConnecting = false;
      setLEDStatus(LED_WIFI_CONNECTING);
    }
    return; // Don't try MQTT if WiFi is down
  } else if (!wifiConnected) {
    Serial.println("WiFi reconnected!");
    wifiConnected = true;
  }
  
  // Check MQTT connection
  if (!mqttConnected && !mqttConnecting && wifiConnected) {
    unsigned long now = millis();
    if (now - lastMQTTAttempt > 10000) { // Try every 10 seconds (increased from 5)
      lastMQTTAttempt = now;
      connectToMQTT();
    }
  }
  
  // Check if MQTT connection is still alive
  if (mqttConnected && !mqttClient.connected()) {
    Serial.println("MQTT connection lost - checking state...");
    Serial.print("MQTT State: ");
    Serial.println(mqttClient.state());
    mqttConnected = false;
    mqttConnecting = false;
    setLEDStatus(LED_MQTT_CONNECTING);
    
    // Add a small delay before reconnecting
    delay(1000);
  }
}

// ===== OTA UPDATE FUNCTIONS =====

void handleFirmwareUpdateMessage(JsonDocument& updateMsg) {
  if (otaInProgress) {
    Serial.println("OTA already in progress, ignoring update message");
    return;
  }
  
  // Extract update information
  String newVersion = updateMsg["version"].as<String>();
  String firmwareUrl = updateMsg["firmware_url"].as<String>();
  String signature = updateMsg["signature"].as<String>();
  bool forceUpdate = updateMsg["force_update"] | false;
  
  Serial.print("Firmware update available: ");
  Serial.println(newVersion);
  Serial.print("Current version: ");
  Serial.println(FIRMWARE_VERSION);
  Serial.print("Force update: ");
  Serial.println(forceUpdate ? "Yes" : "No");
  
  // Check if update is needed
  if (!forceUpdate && !isNewerVersion(newVersion, FIRMWARE_VERSION)) {
    Serial.println("No update needed - current version is up to date");
    publishOTAStatus(OTA_IDLE, "Already up to date");
    return;
  }
  
  Serial.println("Starting OTA update process...");
  publishOTAStatus(OTA_CHECKING, "Update available, starting download");
  
  // Start OTA update
  performOTAUpdate(firmwareUrl, newVersion, signature);
}

bool isNewerVersion(const String& newVersion, const String& currentVersion) {
  // Simple semantic version comparison (e.g., "1.2.1" vs "1.2.0")
  // Split versions by dots and compare each part
  
  int newMajor = 0, newMinor = 0, newPatch = 0;
  int curMajor = 0, curMinor = 0, curPatch = 0;
  
  // Parse new version
  sscanf(newVersion.c_str(), "%d.%d.%d", &newMajor, &newMinor, &newPatch);
  
  // Parse current version
  sscanf(currentVersion.c_str(), "%d.%d.%d", &curMajor, &curMinor, &curPatch);
  
  // Compare versions
  if (newMajor > curMajor) return true;
  if (newMajor < curMajor) return false;
  
  if (newMinor > curMinor) return true;
  if (newMinor < curMinor) return false;
  
  if (newPatch > curPatch) return true;
  
  return false; // Equal or older
}

void performOTAUpdate(const String& firmwareUrl, const String& version, const String& signature) {
  otaInProgress = true;
  currentOTAStatus = OTA_DOWNLOADING;
  
  Serial.println("=== Starting OTA Update ===");
  Serial.print("Downloading from: ");
  Serial.println(firmwareUrl);
  
  publishOTAStatus(OTA_DOWNLOADING, "Downloading firmware...");
  setLEDStatus(LED_OTA_DOWNLOADING);
  
  size_t downloadedSize = 0;
  if (!downloadFirmware(firmwareUrl, downloadedSize)) {
    Serial.println("Firmware download failed");
    publishOTAStatus(OTA_FAILED, "Download failed");
    otaInProgress = false;
    currentOTAStatus = OTA_IDLE;
    return;
  }
  
  Serial.println("Firmware downloaded successfully");
  publishOTAStatus(OTA_VALIDATING, "Validating firmware signature...");
  setLEDStatus(LED_OTA_VALIDATING);
  
  // Validate firmware signature
  if (!validateFirmwareSignature(signature, downloadedSize)) {
    Serial.println("Firmware signature validation failed!");
    publishOTAStatus(OTA_FAILED, "Signature validation failed");
    Update.abort();
    otaInProgress = false;
    currentOTAStatus = OTA_IDLE;
    return;
  }
  
  Serial.println("Firmware signature validated successfully");
  publishOTAStatus(OTA_INSTALLING, "Installing firmware...");
  setLEDStatus(LED_OTA_INSTALLING);
  
  // Finalize the update
  if (Update.end(true)) {
    Serial.println("OTA Update successful! Rebooting...");
    publishOTAStatus(OTA_SUCCESS, "Update successful, rebooting...");
    delay(1000);
    ESP.restart();
  } else {
    Serial.print("OTA Update failed: ");
    Serial.println(Update.errorString());
    publishOTAStatus(OTA_FAILED, "Installation failed: " + String(Update.errorString()));
    otaInProgress = false;
    currentOTAStatus = OTA_IDLE;
  }
}

bool downloadFirmware(const String& url, size_t& downloadedSize) {
  HTTPClient http;
  WiFiClientSecure client;
  
  // For HTTPS, configure client
  if (url.startsWith("https://")) {
    client.setInsecure(); // For now, skip certificate validation (configure proper CA in production)
    http.begin(client, url);
  } else {
    http.begin(url);
  }
  
  // Set timeout
  http.setTimeout(30000); // 30 seconds
  
  int httpCode = http.GET();
  
  if (httpCode != HTTP_CODE_OK) {
    Serial.print("HTTP GET failed: ");
    Serial.println(httpCode);
    http.end();
    return false;
  }
  
  int contentLength = http.getSize();
  Serial.print("Firmware size: ");
  Serial.println(contentLength);
  
  if (contentLength <= 0) {
    Serial.println("Invalid content length");
    http.end();
    return false;
  }
  
  // Begin OTA update
  if (!Update.begin(contentLength)) {
    Serial.print("OTA begin failed: ");
    Serial.println(Update.errorString());
    http.end();
    return false;
  }
  
  // Get stream for reading
  WiFiClient* stream = http.getStreamPtr();
  size_t written = 0;
  uint8_t buffer[512];
  
  Serial.println("Starting firmware download...");
  
  while (http.connected() && (written < contentLength)) {
    size_t available = stream->available();
    if (available) {
      size_t readBytes = stream->readBytes(buffer, min(available, sizeof(buffer)));
      
      if (readBytes > 0) {
        size_t writtenBytes = Update.write(buffer, readBytes);
        if (writtenBytes != readBytes) {
          Serial.println("Write error during OTA update");
          http.end();
          return false;
        }
        written += writtenBytes;
        
        // Progress reporting
        if (written % 10240 == 0) { // Every 10KB
          int progress = (written * 100) / contentLength;
          Serial.print("Progress: ");
          Serial.print(progress);
          Serial.println("%");
          
          // Update Neopixel progress bar
          setNeopixelProgress((float)progress / 100.0, CRGB::Orange);
        }
      }
    }
    delay(1);
  }
  
  http.end();
  downloadedSize = written;
  
  Serial.print("Download completed: ");
  Serial.print(written);
  Serial.println(" bytes");
  
  return (written == contentLength);
}

bool validateFirmwareSignature(const String& signature, size_t firmwareSize) {
  // For now, implement a simple SHA256 checksum validation
  // In production, you should implement proper RSA/ECDSA signature verification
  
  Serial.println("Calculating firmware hash...");
  String calculatedHash = calculateFirmwareSHA256();
  
  Serial.print("Calculated SHA256: ");
  Serial.println(calculatedHash);
  Serial.print("Expected signature: ");
  Serial.println(signature);
  
  // Simple hash comparison (in production, verify RSA signature)
  return (calculatedHash == signature);
}

String calculateFirmwareSHA256() {
  // Calculate SHA256 of the downloaded firmware in the OTA partition
  const esp_partition_t* update_partition = esp_ota_get_next_update_partition(NULL);
  if (!update_partition) {
    Serial.println("Could not find update partition");
    return "";
  }
  
  mbedtls_sha256_context sha256_ctx;
  mbedtls_sha256_init(&sha256_ctx);
  mbedtls_sha256_starts(&sha256_ctx, 0); // 0 = SHA256, not SHA224
  
  uint8_t buffer[1024];
  size_t bytes_read = 0;
  size_t total_size = Update.size();
  
  for (size_t offset = 0; offset < total_size; offset += sizeof(buffer)) {
    size_t bytes_to_read = min(sizeof(buffer), total_size - offset);
    
    if (esp_partition_read(update_partition, offset, buffer, bytes_to_read) == ESP_OK) {
      mbedtls_sha256_update(&sha256_ctx, buffer, bytes_to_read);
      bytes_read += bytes_to_read;
    } else {
      Serial.println("Failed to read partition for hash calculation");
      mbedtls_sha256_free(&sha256_ctx);
      return "";
    }
  }
  
  uint8_t hash[32];
  mbedtls_sha256_finish(&sha256_ctx, hash);
  mbedtls_sha256_free(&sha256_ctx);
  
  // Convert hash to hex string
  String hashString = "";
  for (int i = 0; i < 32; i++) {
    if (hash[i] < 16) hashString += "0";
    hashString += String(hash[i], HEX);
  }
  
  return hashString;
}

void publishOTAStatus(OTAStatus status, const String& message) {
  JsonDocument doc;
  doc["device_id"] = DEVICE_ID;
  doc["firmware_version"] = FIRMWARE_VERSION;
  doc["timestamp"] = getCurrentUnixTime();
  
  String statusText = "";
  switch (status) {
    case OTA_IDLE: statusText = "idle"; break;
    case OTA_CHECKING: statusText = "checking"; break;
    case OTA_DOWNLOADING: statusText = "downloading"; break;
    case OTA_VALIDATING: statusText = "validating"; break;
    case OTA_INSTALLING: statusText = "installing"; break;
    case OTA_SUCCESS: statusText = "success"; break;
    case OTA_FAILED: statusText = "failed"; break;
  }
  
  doc["ota_status"] = statusText;
  doc["message"] = message;
  doc["free_heap"] = ESP.getFreeHeap();
  
  String jsonString;
  serializeJson(doc, jsonString);
  
  String otaStatusTopic = deviceHostname + "/ota/status";
  if (mqttClient.connected()) {
    // Flash white for outgoing message
    setLEDStatus(LED_MQTT_SENDING);
    lastMQTTSend = millis();
    
    mqttClient.publish(otaStatusTopic.c_str(), jsonString.c_str());
  }
  
  Serial.print("OTA Status: ");
  Serial.print(statusText);
  Serial.print(" - ");
  Serial.println(message);
}

void checkForFirmwareUpdates() {
  // This function could actively check a server endpoint for updates
  // For now, we just rely on MQTT messages
  Serial.println("Checking for firmware updates...");
  
  // You could implement HTTP polling here:
  // - GET request to firmware server API
  // - Compare available version with current
  // - Trigger update if newer version available
  
  publishOTAStatus(OTA_CHECKING, "Periodic update check");
}

// ===== NEOPIXEL FUNCTIONS =====

void initializeNeopixels() {
  Serial.println("Initializing Neopixels...");
  Serial.print("Neopixel pin: ");
  Serial.println(NEOPIXEL_PIN);
  Serial.print("Number of LEDs: ");
  Serial.println(NUM_LEDS);
  
  FastLED.addLeds<WS2812B, NEOPIXEL_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(brightness);
  FastLED.clear();
  FastLED.show();
  
  // Startup animation - rainbow sweep
  Serial.println("Neopixel startup animation...");
  for (int i = 0; i < NUM_LEDS; i++) {
    leds[i] = CHSV(i * 255 / NUM_LEDS, 255, 255);
    FastLED.show();
    delay(20);
  }
  delay(500);
  
  // Clear all LEDs
  fill_solid(leds, NUM_LEDS, CRGB::Black);
  FastLED.show();
  Serial.println("Neopixels initialized");
}

void setNeopixelColor(CRGB color, uint8_t brightness) {
  currentColor = color;
  fill_solid(leds, NUM_LEDS, color);
  FastLED.setBrightness(brightness);
  FastLED.show();
}

void setNeopixelProgress(float progress, CRGB color) {
  // Progress bar from 0.0 to 1.0
  int numLit = (int)(progress * NUM_LEDS);
  
  fill_solid(leds, NUM_LEDS, CRGB::Black);
  for (int i = 0; i < numLit && i < NUM_LEDS; i++) {
    leds[i] = color;
  }
  FastLED.show();
}

void neopixelSpinning(CRGB color, uint32_t speed) {
  static unsigned long lastUpdate = 0;
  static uint8_t position = 0;
  
  if (millis() - lastUpdate > speed) {
    fill_solid(leds, NUM_LEDS, CRGB::Black);
    
    // Create a spinning effect with 3 LEDs
    for (int i = 0; i < 3; i++) {
      int pos = (position + i * (NUM_LEDS / 3)) % NUM_LEDS;
      leds[pos] = color;
    }
    
    position = (position + 1) % NUM_LEDS;
    FastLED.show();
    lastUpdate = millis();
  }
}

void neopixelPulse(CRGB color, uint32_t speed) {
  static unsigned long lastUpdate = 0;
  static uint8_t brightness = 0;
  static int8_t direction = 1;
  
  if (millis() - lastUpdate > speed / 255) {
    brightness += direction * 5;
    if (brightness >= 250) {
      brightness = 250;
      direction = -1;
    } else if (brightness <= 5) {
      brightness = 5;
      direction = 1;
    }
    
    fill_solid(leds, NUM_LEDS, color);
    FastLED.setBrightness(brightness);
    FastLED.show();
    lastUpdate = millis();
  }
}

void neopixelBlink(CRGB color, uint32_t interval) {
  static unsigned long lastBlink = 0;
  static bool state = false;
  
  if (millis() - lastBlink > interval) {
    if (state) {
      fill_solid(leds, NUM_LEDS, color);
    } else {
      fill_solid(leds, NUM_LEDS, CRGB::Black);
    }
    state = !state;
    FastLED.show();
    lastBlink = millis();
  }
}

void neopixelRainbow(uint8_t speed) {
  static unsigned long lastUpdate = 0;
  static uint8_t hue = 0;
  
  if (millis() - lastUpdate > speed) {
    for (int i = 0; i < NUM_LEDS; i++) {
      leds[i] = CHSV(hue + (i * 255 / NUM_LEDS), 255, 255);
    }
    hue += 1;
    FastLED.show();
    lastUpdate = millis();
  }
}

void neopixelTemperatureFlash() {
  // Brief white flash for temperature reading
  fill_solid(leds, NUM_LEDS, CRGB::White);
  FastLED.setBrightness(100);
  FastLED.show();
  delay(50);
  fill_solid(leds, NUM_LEDS, CRGB::Black);
  FastLED.show();
  delay(50);
}

void neopixelBreathe(CRGB color, uint32_t speed) {
  unsigned long now = millis();
  
  // Much smoother animation - update every 20ms
  if (now - breathingLastUpdate > 20) {
    // Slower, smoother expansion changes
    float increment = 60.0 / (speed / 20.0); // Calculate smooth increment based on speed
    breathingBrightness += breathingDirection * increment;
    
    if (breathingBrightness >= 30.0) {  // Max expansion radius (half the strip)
      breathingBrightness = 30.0;
      breathingDirection = -1.0;
    } else if (breathingBrightness <= 0.0) {  // Contract to center
      breathingBrightness = 0.0;
      breathingDirection = 1.0;
    }
    
    // Clear all LEDs first
    fill_solid(leds, NUM_LEDS, CRGB::Black);
    
    // Calculate center and expansion radius with sine wave smoothing
    int center = NUM_LEDS / 2; // Center LED (30 for 60-LED strip)
    float phase = breathingBrightness / 30.0; // Normalize to 0-1
    float sineWave = sin(phase * PI); // Smooth sine transition
    int radius = (int)(sineWave * breathingBrightness);
    
    // Light LEDs from center outward
    for (int i = 0; i <= radius; i++) {
      // Light LEDs on both sides of center
      int leftLED = center - i;
      int rightLED = center + i;
      
      // Create a subtle fade towards the edges for smoother appearance
      float edgeFade = 1.0 - (float(i) / float(radius + 1)) * 0.3; // 30% fade at edges
      CRGB fadedColor = color;
      fadedColor.fadeToBlackBy(255 - (uint8_t)(80 * edgeFade)); // Moderate brightness with edge fade
      
      // Set the LEDs if they're within bounds
      if (leftLED >= 0) {
        leds[leftLED] = fadedColor;
      }
      if (rightLED < NUM_LEDS && rightLED != leftLED) {
        leds[rightLED] = fadedColor;
      }
    }
    
    FastLED.show();
    breathingLastUpdate = now;
  }
}

void resetBreathingAnimation() {
  // Reset breathing animation variables for smooth restart
  breathingLastUpdate = 0;
  breathingBrightness = 0.0;
  breathingDirection = 1.0;
  Serial.println("Breathing animation reset");
}

void neopixelFlash(CRGB color, uint32_t duration) {
  // Immediate bright flash that interrupts breathing
  fill_solid(leds, NUM_LEDS, color);
  FastLED.setBrightness(180); // Slightly brighter flash for better visibility
  FastLED.show();
}

void updateNeopixels() {
  // Handle activity flash sequence: Flash → Black pause → Resume breathing
  unsigned long now = millis();
  
  // After flash duration, go to blackout
  if ((currentLEDStatus == LED_MQTT_SENDING && now - lastMQTTSend > MQTT_FLASH_DURATION) ||
      (currentLEDStatus == LED_MQTT_RECEIVING && now - lastMQTTReceive > MQTT_FLASH_DURATION)) {
    // Go to black pause state
    inMQTTBlackout = true;
    setLEDStatus(LED_OFF);
    Serial.println("Entering MQTT blackout period");
    return;
  }
  
  // After blackout period, resume breathing  
  if (inMQTTBlackout && currentLEDStatus == LED_OFF && mqttConnected) {
    unsigned long timeSinceLastActivity = max(now - lastMQTTSend, now - lastMQTTReceive);
    if (timeSinceLastActivity > MQTT_FLASH_DURATION + MQTT_BLACKOUT_DURATION) {
      inMQTTBlackout = false;
      Serial.println("Resuming breathing after blackout");
      setLEDStatus(LED_MQTT_CONNECTED);
      resetBreathingAnimation();
      return;
    }
  }
  
  switch (currentLEDStatus) {
    case LED_OFF:
      setNeopixelColor(CRGB::Black, 0);
      break;
      
    case LED_WIFI_CONNECTING:
      neopixelBlink(CRGB::Blue, 1000); // Slow blue blink
      break;
      
    case LED_MQTT_CONNECTING:
      neopixelBlink(CRGB::Yellow, 250); // Fast yellow blink
      break;
      
    case LED_MQTT_CONNECTED:
      neopixelBreathe(CRGB::Green, 5000); // Slower, more zen-like breathing (5 seconds)
      break;
      
    case LED_MQTT_SENDING:
      neopixelFlash(CRGB::White, 150); // Brief white flash
      break;
      
    case LED_MQTT_RECEIVING:
      neopixelFlash(CRGB::Red, 150); // Brief red flash
      break;
      
    case LED_OTA_DOWNLOADING:
      // This will be updated with actual progress in OTA functions
      neopixelSpinning(CRGB::Orange, 100); // Orange spinning for now
      break;
      
    case LED_OTA_VALIDATING:
      neopixelSpinning(CRGB::Cyan, 50); // Fast cyan spinning
      break;
      
    case LED_OTA_INSTALLING:
      neopixelPulse(CRGB::Red, 200); // Fast red pulse
      break;
      
    case LED_TEMPERATURE_READ:
      neopixelTemperatureFlash(); // Brief white flash
      break;
      
    case LED_ERROR:
      neopixelBlink(CRGB::Red, 200); // Fast red blink
      break;
  }
}
