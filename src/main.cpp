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

#ifdef ESP32
#include "esp_camera.h"
// JPEG capture tuning (used when temporarily reinitializing camera to JPEG)
// Choose a larger frame size and better JPEG quality for ambiguous-frame uploads.
// FRAMESIZE_* constants: QQVGA(160x120), QCIF, QVGA(320x240), VGA(640x480), SVGA, XGA, SXGA, UXGA
const framesize_t CAMERA_JPEG_FRAME_SIZE = FRAMESIZE_VGA;
const int CAMERA_JPEG_QUALITY = 8; // 0 = highest quality, 63 = lowest quality
const int CAMERA_JPEG_XCLK_HZ = 20000000;
#endif

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

// Attachment configuration
enum AttachmentType {
  ATTACHMENT_NEOPIXEL,
  ATTACHMENT_BUILTIN,
  ATTACHMENT_RGB,
  ATTACHMENT_TRAFFIC
};

// Select the attachment type used on the board here:
const AttachmentType ATTACHMENT = ATTACHMENT_BUILTIN; // change as needed

// Pins for RGB / Traffic light attachments (examples: D7,D8,D9)
const int RGB_R_PIN = D7;
const int RGB_G_PIN = D8;
const int RGB_B_PIN = D9;

const int TRAFFIC_RED_PIN = D7;
const int TRAFFIC_YELLOW_PIN = D8;
const int TRAFFIC_GREEN_PIN = D9;

// Sensor configuration
enum SensorType {
  SENSOR_DHT_TEMP,        // Temperature and Humidity (DHT22 or similar)
  SENSOR_MQ7_GAS,         // MQ7 Gas Sensor (CO detector)
  SENSOR_SENSE,           // Overhead doorway sense (two-beam direction sensor)
  SENSOR_SENSE_CAMERA     // Camera-based overhead doorway sensor (OV5640/OV2640)
};

// Select the sensor type used on the board here:
const SensorType SENSOR = SENSOR_SENSE_CAMERA; // change as needed

// Pin definitions for sensors
const int DHT_PIN = D0;           // GPIO1 (D0 on XIAO) - DHT temperature/humidity sensor
const int MQ7_ANALOG_PIN = A1;    // GPIO3 (A1 on XIAO) - MQ7 analog output
const int MQ7_DIGITAL_PIN = D2;   // GPIO26 (D2 on XIAO) - MQ7 digital output (threshold)

// Pins for SENSOR_SENSE (two sensors to determine direction)
// Place these above the doorway, offset so that an approaching person trips
// the sensors sequentially. Change pins as needed for your wiring.
const int SENSE_A_PIN = D3; // Sensor A (e.g., closer to entry side)
const int SENSE_B_PIN = D4; // Sensor B (e.g., closer to exit side)
const unsigned long SENSE_TIMEOUT_MS = 1000; // Time window (ms) to consider sequential triggers
const bool SENSE_ACTIVE_LOW = true; // If sensors use pull-ups and go LOW when triggered

// Camera settings (OV5640/OV2640 via ESP32-CAM connector)
// NOTE: These are typical ESP32-CAM pin mappings — adjust to your wiring.
const bool CAMERA_ENABLED_BY_DEFAULT = true;
// XIAO ESP32S3 Sense card slot pin mapping (DVP signals)
const int CAM_PIN_PWDN = -1;
const int CAM_PIN_RESET = -1;
const int CAM_PIN_XCLK = 10;   // XMCLK
const int CAM_PIN_PCLK = 13;   // DVP_PCLK
const int CAM_PIN_VSYNC = 38;  // DVP_VSYNC
const int CAM_PIN_HREF = 47;   // DVP_HREF

// SCCB (I2C) for camera configuration
const int CAM_PIN_SIOD = 40;   // CAM_SDA
const int CAM_PIN_SIOC = 39;   // CAM_SCL

// Data lines mapped to DVP_Y2..DVP_Y9 on the XIAO ESP32S3 Sense slot
// D0..D7 -> DVP_Y2..DVP_Y9 respectively
const int CAM_PIN_D0 = 15; // DVP_Y2 (GPIO15)
const int CAM_PIN_D1 = 17; // DVP_Y3 (GPIO17)
const int CAM_PIN_D2 = 18; // DVP_Y4 (GPIO18)
const int CAM_PIN_D3 = 16; // DVP_Y5 (GPIO16)
const int CAM_PIN_D4 = 14; // DVP_Y6 (GPIO14)
const int CAM_PIN_D5 = 12; // DVP_Y7 (GPIO12)
const int CAM_PIN_D6 = 11; // DVP_Y8 (GPIO11)
const int CAM_PIN_D7 = 48; // DVP_Y9 (GPIO48)

// Frame-diff parameters
const int CAM_FRAME_WIDTH = 160;
const int CAM_FRAME_HEIGHT = 120;
const int CAM_DIFF_THRESHOLD = 30; // per-pixel threshold
const int CAM_MOTION_MIN_PIXELS = 200; // min changed pixels to consider motion
const int CAM_AMBIGUOUS_PIXELS = 800; // above this, consider ambiguous and send frame

// Where ambiguous/full frames are published (MQTT topic base). The actual
// topic used is `deviceHostname + "/" + CAMERA_FRAME_TOPIC_BASE`.
const char CAMERA_FRAME_TOPIC_BASE[] = "camera/frames/motion/ambiguous";
// Optional HTTP endpoint to POST ambiguous frames to (empty = disabled)
const char CAMERA_FRAME_HTTP_ENDPOINT[] = ""; // e.g. "https://example.com/frames"

// Frame upload method selection
enum FrameUploadMethod {
  UPLOAD_NONE,
  UPLOAD_HTTP_JWT, // Multi-part upload to your endpoint using JWT (default)
  UPLOAD_NTFY      // Direct POST to ntfy.sh style endpoint (binary body)
};

// Choose which method to use for HTTP uploads
const FrameUploadMethod CAMERA_FRAME_UPLOAD_METHOD = UPLOAD_NONE;

// If using NTFY, you can set a token here (optional). The endpoint should be
// something like "https://ntfy.sh/your-topic". If token is set, it will be
// sent as `Authorization: Bearer <token>`.
const char CAMERA_NTFY_TOKEN[] = "";

// JPEG capture tuning constants are defined in the ESP32 camera section

// Camera runtime state
bool cameraAvailable = false;
uint8_t* prevFrame = NULL;
uint8_t* currFrame = NULL;
int prevMotionX = -1;

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
// Temperature sensor objects (initialized only if selected)
#if defined(__AVR__) || defined(ESP32)
  #include <DHT.h>
  #define DHT_TYPE DHT22
#endif

OneWire oneWire(DHT_PIN);  // Note: using DHT_PIN instead of TEMP_SENSOR_PIN
DallasTemperature sensors(&oneWire);  // For DHT_TEMP mode (if using OneWire)

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

// Attachment-agnostic wrappers
void initializeAttachments();
void attachmentSetColor(CRGB color, uint8_t brightness = 50);
void attachmentSetProgress(float progress, CRGB color);
void attachmentSpinning(CRGB color, uint32_t speed = 100);
void attachmentPulse(CRGB color, uint32_t speed = 1000);
void attachmentBlink(CRGB color, uint32_t interval = 500);
void attachmentBreathe(CRGB color, uint32_t speed = 3000);
void attachmentFlash(CRGB color, uint32_t duration = 100);
void attachmentTemperatureFlash();
void updateAttachments();

// Gas level thresholds (for MQ7)
enum GasLevel {
  GAS_LOW,        // < 5 ppm - Green (safe)
  GAS_MEDIUM,     // 5-15 ppm - Yellow (caution)
  GAS_HIGH,       // 15-35 ppm - Orange (warning)
  GAS_CRITICAL    // > 35 ppm - Red (danger)
};

// Sensor-agnostic wrapper functions
void initializeSensor();
struct SensorReadings {
  float temperature;
  float humidity;
  float gasLevel;
  bool success;
  bool digitalThreshold;  // For MQ7 digital output
  GasLevel gasCategoryLevel;  // Categorized gas level
};
SensorReadings readSensor();
void publishSensorData();
// SENSOR_SENSE function declarations
void updateSenseSensor();
void publishTrafficEvent(const char* event, int inCount, int outCount);
// Camera-based sense declarations
bool initializeCameraModule();
bool captureGrayscaleFrame(uint8_t* buf, int w, int h); // fills buf with w*h bytes (0-255)
void processCameraFrame();

// Global variable to track current gas level
GasLevel currentGasLevel = GAS_LOW;

// ===== SENSOR_SENSE (doorway traffic) globals =====
enum SenseState {
  SENSE_IDLE,
  SENSE_A_TRIGGERED,
  SENSE_B_TRIGGERED
};

volatile int senseInCount = 0;
volatile int senseOutCount = 0;
SenseState senseState = SENSE_IDLE;
unsigned long senseLastTriggerTime = 0;

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
  // Early visual boot indicator (useful if Serial is not visible)
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);
  delay(150);
  digitalWrite(LED_PIN, LOW);

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
  initializeAttachments();
  
  // Initialize sensor (DHT or MQ7)
  initializeSensor();
  
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

// ===== Attachment wrapper implementations =====

void initializeAttachments() {
  switch (ATTACHMENT) {
    case ATTACHMENT_NEOPIXEL:
      Serial.println("=== Initializing Neopixel Attachment ===");
      initializeNeopixels();
      break;
    case ATTACHMENT_BUILTIN:
      Serial.println("=== Initializing Built-in LED Attachment ===");
      pinMode(LED_PIN, OUTPUT);
      digitalWrite(LED_PIN, LOW);
      Serial.print("Built-in LED on pin: ");
      Serial.println(LED_PIN);
      break;
    case ATTACHMENT_RGB:
      Serial.println("=== Initializing RGB LED Attachment ===");
      pinMode(RGB_R_PIN, OUTPUT);
      pinMode(RGB_G_PIN, OUTPUT);
      pinMode(RGB_B_PIN, OUTPUT);
      digitalWrite(RGB_R_PIN, LOW);
      digitalWrite(RGB_G_PIN, LOW);
      digitalWrite(RGB_B_PIN, LOW);
      Serial.print("RGB Red pin: ");
      Serial.print(RGB_R_PIN);
      Serial.print(" | Green pin: ");
      Serial.print(RGB_G_PIN);
      Serial.print(" | Blue pin: ");
      Serial.println(RGB_B_PIN);
      break;
    case ATTACHMENT_TRAFFIC:
      Serial.println("=== Initializing Traffic Light Attachment ===");
      pinMode(TRAFFIC_RED_PIN, OUTPUT);
      pinMode(TRAFFIC_YELLOW_PIN, OUTPUT);
      pinMode(TRAFFIC_GREEN_PIN, OUTPUT);
      digitalWrite(TRAFFIC_RED_PIN, LOW);
      digitalWrite(TRAFFIC_YELLOW_PIN, LOW);
      digitalWrite(TRAFFIC_GREEN_PIN, LOW);
      Serial.print("Traffic Red pin: ");
      Serial.print(TRAFFIC_RED_PIN);
      Serial.print(" | Yellow pin: ");
      Serial.print(TRAFFIC_YELLOW_PIN);
      Serial.print(" | Green pin: ");
      Serial.println(TRAFFIC_GREEN_PIN);
      break;
  }
}

static unsigned long _attach_lastBlink = 0;
static bool _attach_blinkState = false;
static unsigned long _attach_lastSpin = 0;
static uint8_t _attach_spinPos = 0;

void attachmentSetColor(CRGB color, uint8_t brightness) {
  if (ATTACHMENT == ATTACHMENT_NEOPIXEL) {
    setNeopixelColor(color, brightness);
    return;
  }

  // For non-Neopixel attachments, map color to digital outputs
  bool isOn = !(color == CRGB::Black);

  if (ATTACHMENT == ATTACHMENT_BUILTIN) {
    digitalWrite(LED_PIN, isOn ? HIGH : LOW);
    return;
  }

  if (ATTACHMENT == ATTACHMENT_RGB) {
    digitalWrite(RGB_R_PIN, color.r > 0 ? HIGH : LOW);
    digitalWrite(RGB_G_PIN, color.g > 0 ? HIGH : LOW);
    digitalWrite(RGB_B_PIN, color.b > 0 ? HIGH : LOW);
    return;
  }

  if (ATTACHMENT == ATTACHMENT_TRAFFIC) {
    // Map primary colors to traffic LEDs
    if (color == CRGB::Red) {
      digitalWrite(TRAFFIC_RED_PIN, HIGH);
      digitalWrite(TRAFFIC_YELLOW_PIN, LOW);
      digitalWrite(TRAFFIC_GREEN_PIN, LOW);
    } else if (color == CRGB::Yellow) {
      digitalWrite(TRAFFIC_RED_PIN, LOW);
      digitalWrite(TRAFFIC_YELLOW_PIN, HIGH);
      digitalWrite(TRAFFIC_GREEN_PIN, LOW);
    } else if (color == CRGB::Green) {
      digitalWrite(TRAFFIC_RED_PIN, LOW);
      digitalWrite(TRAFFIC_YELLOW_PIN, LOW);
      digitalWrite(TRAFFIC_GREEN_PIN, HIGH);
    } else if (color == CRGB::White) {
      digitalWrite(TRAFFIC_RED_PIN, HIGH);
      digitalWrite(TRAFFIC_YELLOW_PIN, HIGH);
      digitalWrite(TRAFFIC_GREEN_PIN, HIGH);
    } else {
      // default: turn all off
      digitalWrite(TRAFFIC_RED_PIN, LOW);
      digitalWrite(TRAFFIC_YELLOW_PIN, LOW);
      digitalWrite(TRAFFIC_GREEN_PIN, LOW);
    }
    return;
  }
}

void attachmentSetProgress(float progress, CRGB color) {
  if (ATTACHMENT == ATTACHMENT_NEOPIXEL) {
    setNeopixelProgress(progress, color);
    return;
  }

  // For simple attachments, show color while progress > 0
  if (progress > 0.001) {
    attachmentSetColor(color);
  } else {
    attachmentSetColor(CRGB::Black);
  }
}

void attachmentBlink(CRGB color, uint32_t interval) {
  if (ATTACHMENT == ATTACHMENT_NEOPIXEL) {
    neopixelBlink(color, interval);
    return;
  }

  unsigned long now = millis();
  if (now - _attach_lastBlink > interval) {
    _attach_blinkState = !_attach_blinkState;
    _attach_lastBlink = now;
    if (_attach_blinkState) {
      attachmentSetColor(color);
    } else {
      attachmentSetColor(CRGB::Black);
    }
  }
}

void attachmentPulse(CRGB color, uint32_t speed) {
  if (ATTACHMENT == ATTACHMENT_NEOPIXEL) {
    neopixelPulse(color, speed);
    return;
  }

  // Fallback to blink with faster interval
  attachmentBlink(color, max(20U, speed / 10));
}

void attachmentSpinning(CRGB color, uint32_t speed) {
  if (ATTACHMENT == ATTACHMENT_NEOPIXEL) {
    neopixelSpinning(color, speed);
    return;
  }

  // For RGB/traffic, rotate through R->G->B
  unsigned long now = millis();
  if (now - _attach_lastSpin > speed) {
    _attach_spinPos = (_attach_spinPos + 1) % 3;
    _attach_lastSpin = now;
    if (ATTACHMENT == ATTACHMENT_RGB) {
      digitalWrite(RGB_R_PIN, _attach_spinPos == 0 ? HIGH : LOW);
      digitalWrite(RGB_G_PIN, _attach_spinPos == 1 ? HIGH : LOW);
      digitalWrite(RGB_B_PIN, _attach_spinPos == 2 ? HIGH : LOW);
    } else if (ATTACHMENT == ATTACHMENT_TRAFFIC) {
      digitalWrite(TRAFFIC_RED_PIN, _attach_spinPos == 0 ? HIGH : LOW);
      digitalWrite(TRAFFIC_YELLOW_PIN, _attach_spinPos == 1 ? HIGH : LOW);
      digitalWrite(TRAFFIC_GREEN_PIN, _attach_spinPos == 2 ? HIGH : LOW);
    } else {
      // builtin LED: just toggle
      digitalWrite(LED_PIN, !_attach_spinPos ? HIGH : LOW);
    }
  }
}

void attachmentBreathe(CRGB color, uint32_t speed) {
  if (ATTACHMENT == ATTACHMENT_NEOPIXEL) {
    neopixelBreathe(color, speed);
    return;
  }

  // For non-Neopixel attachments: implement slow blink as breathing substitute
  // Traffic lights, RGB, and built-in LED will pulse the color on/off
  attachmentBlink(color, speed / 2);  // Half the speed for slower breathing effect
}

void attachmentFlash(CRGB color, uint32_t duration) {
  if (ATTACHMENT == ATTACHMENT_NEOPIXEL) {
    neopixelFlash(color, duration);
    return;
  }

  attachmentSetColor(color);
  delay(duration);
  attachmentSetColor(CRGB::Black);
}

void attachmentTemperatureFlash() {
  if (ATTACHMENT == ATTACHMENT_NEOPIXEL) {
    neopixelTemperatureFlash();
    return;
  }
  // Simple brief white flash
  attachmentFlash(CRGB::White, 50);
}

void loop() {
  // Update LED status
  updateLEDStatus();
  
  // Update Neopixels
  updateAttachments();
  // Update doorway sense sensor (if enabled)
  updateSenseSensor();
  
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
    // Subscribe to camera test topic (one-shot capture request)
    String cameraTestTopic = deviceHostname + "/camera/test";
    mqttClient.subscribe(cameraTestTopic.c_str());
    Serial.print("Subscribed to camera test topic: "); Serial.println(cameraTestTopic);
    
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
  
  // Handle camera test request (one-shot capture)
  String cameraTestTopic = deviceHostname + "/camera/test";
  String cameraResponseTopic = deviceHostname + "/camera/test/response";
  if (String(topic) == cameraTestTopic) {
    Serial.println("Received camera test request via MQTT");
    // Capture and publish a JPEG frame if possible
    #ifdef ESP32
    if (!cameraAvailable) {
      Serial.println("Camera not available - cannot run test");
    } else if (!mqttClient.connected()) {
      Serial.println("MQTT not connected - cannot publish test frame");
    } else {
      Serial.println("Attempting one-shot JPEG capture for MQTT test...");
      camera_fb_t* fb = esp_camera_fb_get();
      if (fb && fb->format == PIXFORMAT_JPEG) {
        Serial.print("Publishing JPEG frame (size="); Serial.print(fb->len); Serial.println(")");
        mqttClient.publish(cameraResponseTopic.c_str(), (const uint8_t*)fb->buf, fb->len);
        esp_camera_fb_return(fb);
      } else {
        if (fb) { esp_camera_fb_return(fb); fb = NULL; }
        Serial.println("Reinitializing camera to JPEG mode for one-shot test...");
        esp_err_t deinit_r = esp_camera_deinit();
        Serial.print("esp_camera_deinit() returned: "); Serial.println(deinit_r);
        delay(100);
        camera_config_t jpegConfig;
        memset(&jpegConfig, 0, sizeof(jpegConfig));
        jpegConfig.ledc_channel = LEDC_CHANNEL_0;
        jpegConfig.ledc_timer = LEDC_TIMER_0;
        jpegConfig.pin_d0 = CAM_PIN_D0;
        jpegConfig.pin_d1 = CAM_PIN_D1;
        jpegConfig.pin_d2 = CAM_PIN_D2;
        jpegConfig.pin_d3 = CAM_PIN_D3;
        jpegConfig.pin_d4 = CAM_PIN_D4;
        jpegConfig.pin_d5 = CAM_PIN_D5;
        jpegConfig.pin_d6 = CAM_PIN_D6;
        jpegConfig.pin_d7 = CAM_PIN_D7;
        jpegConfig.pin_xclk = CAM_PIN_XCLK;
        jpegConfig.pin_pclk = CAM_PIN_PCLK;
        jpegConfig.pin_vsync = CAM_PIN_VSYNC;
        jpegConfig.pin_href = CAM_PIN_HREF;
        jpegConfig.pin_sccb_sda = CAM_PIN_SIOD;
        jpegConfig.pin_sccb_scl = CAM_PIN_SIOC;
        jpegConfig.pin_pwdn = CAM_PIN_PWDN;
        jpegConfig.pin_reset = CAM_PIN_RESET;
        jpegConfig.xclk_freq_hz = CAMERA_JPEG_XCLK_HZ;
        jpegConfig.pixel_format = PIXFORMAT_JPEG;
        jpegConfig.frame_size = CAMERA_JPEG_FRAME_SIZE;
        jpegConfig.jpeg_quality = CAMERA_JPEG_QUALITY;
        jpegConfig.fb_count = 1;

        esp_err_t r = esp_camera_init(&jpegConfig);
        if (r == ESP_OK) {
          camera_fb_t* jfb = esp_camera_fb_get();
          if (jfb) {
            Serial.print("Captured JPEG frame for test, size="); Serial.println(jfb->len);
            mqttClient.publish(cameraResponseTopic.c_str(), (const uint8_t*)jfb->buf, jfb->len);
            esp_camera_fb_return(jfb);
          } else {
            Serial.println("Failed to capture JPEG frame after reinit for test");
          }
          // Restore RGB565 mode used for frame-diff
          Serial.println("Restoring camera to RGB565 mode after test...");
          esp_camera_deinit();
          delay(100);
          camera_config_t rgbConfig;
          memset(&rgbConfig, 0, sizeof(rgbConfig));
          rgbConfig.ledc_channel = LEDC_CHANNEL_0;
          rgbConfig.ledc_timer = LEDC_TIMER_0;
          rgbConfig.pin_d0 = CAM_PIN_D0;
          rgbConfig.pin_d1 = CAM_PIN_D1;
          rgbConfig.pin_d2 = CAM_PIN_D2;
          rgbConfig.pin_d3 = CAM_PIN_D3;
          rgbConfig.pin_d4 = CAM_PIN_D4;
          rgbConfig.pin_d5 = CAM_PIN_D5;
          rgbConfig.pin_d6 = CAM_PIN_D6;
          rgbConfig.pin_d7 = CAM_PIN_D7;
          rgbConfig.pin_xclk = CAM_PIN_XCLK;
          rgbConfig.pin_pclk = CAM_PIN_PCLK;
          rgbConfig.pin_vsync = CAM_PIN_VSYNC;
          rgbConfig.pin_href = CAM_PIN_HREF;
          rgbConfig.pin_sccb_sda = CAM_PIN_SIOD;
          rgbConfig.pin_sccb_scl = CAM_PIN_SIOC;
          rgbConfig.pin_pwdn = CAM_PIN_PWDN;
          rgbConfig.pin_reset = CAM_PIN_RESET;
          rgbConfig.xclk_freq_hz = 20000000;
          rgbConfig.pixel_format = PIXFORMAT_RGB565;
          rgbConfig.frame_size = FRAMESIZE_QQVGA;
          rgbConfig.jpeg_quality = 12;
          rgbConfig.fb_count = 1;
          esp_err_t r2 = esp_camera_init(&rgbConfig);
          if (r2 != ESP_OK) {
            Serial.print("Failed to restore RGB565 camera mode after test: "); Serial.println(r2);
            cameraAvailable = false;
          }
        } else {
          Serial.print("Failed to init camera in JPEG mode for test: "); Serial.println(r);
        }
      }
    }
    #else
    Serial.println("Camera test requested but platform is not ESP32");
    #endif
    return; // handled camera test
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
  Serial.println("=== Reading Sensor Data ===");
  
  // Read sensor data (DHT or MQ7)
  SensorReadings reading = readSensor();
  
  if (reading.success) {
    // Create JSON payload with available readings
    JsonDocument doc;
    doc["device_id"] = DEVICE_ID;
    doc["timestamp"] = getCurrentUnixTime();
    
    if (SENSOR == SENSOR_DHT_TEMP) {
      Serial.print("Temperature: ");
      Serial.print(reading.temperature);
      Serial.println("°C");
      Serial.print("Humidity: ");
      Serial.print(reading.humidity);
      Serial.println("%");
      doc["temperature"] = reading.temperature;
      doc["humidity"] = reading.humidity;
      doc["unit"] = "celsius";
    } else if (SENSOR == SENSOR_MQ7_GAS) {
      Serial.print("Gas Level (ppm): ");
      Serial.println(reading.gasLevel);
      doc["sensor_type"] = "MQ7";
      doc["gas_type"] = "CO";
      doc["ppm"] = reading.gasLevel;
      doc["unit"] = "ppm";
    }
    
    String jsonString;
    serializeJson(doc, jsonString);
    
    // Flash white for outgoing message
    setLEDStatus(LED_MQTT_SENDING);
    lastMQTTSend = millis();
    
    // Publish to MQTT with sensor-specific topic
    String sensorTopic;
    if (SENSOR == SENSOR_DHT_TEMP) {
      sensorTopic = deviceHostname + "/sensors/temperature";
    } else if (SENSOR == SENSOR_MQ7_GAS) {
      sensorTopic = deviceHostname + "/sensors/airquality";
    }
    
    mqttClient.publish(sensorTopic.c_str(), jsonString.c_str());
    Serial.print("Published to: ");
    Serial.println(sensorTopic);
  } else {
    Serial.println("Sensor read error!");
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
    Serial.print("=== LED Status Changed to: ");
    switch (status) {
      case LED_OFF: Serial.println("OFF ==="); break;
      case LED_WIFI_CONNECTING: Serial.println("WiFi Connecting (blue blink) ==="); break;
      case LED_MQTT_CONNECTING: Serial.println("MQTT Connecting (yellow blink) ==="); break;
      case LED_MQTT_CONNECTED: Serial.println("MQTT Connected (green breathing) ==="); break;
      case LED_MQTT_SENDING: Serial.println("MQTT Sending (white flash) ==="); break;
      case LED_MQTT_RECEIVING: Serial.println("MQTT Receiving (red flash) ==="); break;
      case LED_OTA_DOWNLOADING: Serial.println("OTA Downloading (orange progress) ==="); break;
      case LED_OTA_VALIDATING: Serial.println("OTA Validating (cyan spin) ==="); break;
      case LED_OTA_INSTALLING: Serial.println("OTA Installing (red pulse) ==="); break;
      case LED_TEMPERATURE_READ: Serial.println("Temperature Reading (white flash) ==="); break;
      case LED_ERROR: Serial.println("Error (red blink) ==="); break;
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
          
          // Update attachment progress bar (Neopixel or fallback)
          attachmentSetProgress((float)progress / 100.0, CRGB::Orange);
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

// ===== SENSOR FUNCTIONS =====

// MQ7 Calibration parameters
const float MQ7_RL = 10.0;        // Load resistance in kOhms
const float MQ7_RO = 10.0;        // R0 calibration (can be adjusted)
const float MQ7_VCC = 3.3;        // Supply voltage
const float MQ7_ADC_MAX = 4095.0; // ESP32 ADC resolution

// MQ7 Gas Response Curve Parameters for CO (ppm)
// Sensitivity: Rs/R0 = a * (ppm)^b
const float CO_CURVE_A = 116.6;   // Curve coefficient
const float CO_CURVE_B = -2.769;  // Curve exponent

// NOTE: To calibrate MQ7 R0 value:
// 1. Expose sensor to clean air only
// 2. Measure the ADC value and voltage
// 3. Calculate Rs using: Rs = RL * (VCC / Vout - 1)
// 4. In clean air, Rs/R0 should be approximately 0.9-1.0
// 5. Adjust MQ7_RO until Rs/R0 ≈ 0.9 when in clean air
// 6. Then use the sensor for measurement

void initializeSensor() {
  switch (SENSOR) {
    case SENSOR_DHT_TEMP:
      sensors.begin();
      Serial.println("DHT Temperature/Humidity sensor initialized on pin D0");
      break;
      
    case SENSOR_MQ7_GAS:
      pinMode(MQ7_ANALOG_PIN, INPUT);
      pinMode(MQ7_DIGITAL_PIN, INPUT);
      Serial.println("=== MQ7 Gas Sensor Initialization ===");
      Serial.print("Analog input pin: ");
      Serial.println(MQ7_ANALOG_PIN);
      Serial.print("Digital input pin (threshold): ");
      Serial.println(MQ7_DIGITAL_PIN);
      Serial.println("MQ7 Sensor Parameters:");
      Serial.print("  Load Resistance (RL): ");
      Serial.print(MQ7_RL);
      Serial.println(" kΩ");
      Serial.print("  Calibration R0: ");
      Serial.print(MQ7_RO);
      Serial.println(" kΩ");
      Serial.println("Note: MQ7 requires warm-up time for accurate readings");
      Serial.println("Sensor will stabilize after 30-60 seconds of operation");
      break;

    case SENSOR_SENSE: {
      // Configure two digital inputs for the overhead doorway sense
      if (SENSE_ACTIVE_LOW) {
        pinMode(SENSE_A_PIN, INPUT_PULLUP);
        pinMode(SENSE_B_PIN, INPUT_PULLUP);
      } else {
        pinMode(SENSE_A_PIN, INPUT);
        pinMode(SENSE_B_PIN, INPUT);
      }
      senseState = SENSE_IDLE;
      senseInCount = 0;
      senseOutCount = 0;
      Serial.println("=== SENSOR_SENSE (doorway) initialized ===");
      Serial.print("Sense A pin: "); Serial.println(SENSE_A_PIN);
      Serial.print("Sense B pin: "); Serial.println(SENSE_B_PIN);
      Serial.print("Sense active low: "); Serial.println(SENSE_ACTIVE_LOW ? "Yes" : "No");
      break;
    }
    
    case SENSOR_SENSE_CAMERA: {
      // Try to initialize camera module (pins may need adjustment)
      Serial.println("=== SENSOR_SENSE_CAMERA initialization ===");
      cameraAvailable = initializeCameraModule();
      if (cameraAvailable) {
        // Allocate frame buffers for grayscale frames
        prevFrame = (uint8_t*)malloc(CAM_FRAME_WIDTH * CAM_FRAME_HEIGHT);
        currFrame = (uint8_t*)malloc(CAM_FRAME_WIDTH * CAM_FRAME_HEIGHT);
        Serial.print("Attempted allocation of camera buffers. prev=");
        Serial.print((uintptr_t)prevFrame, HEX);
        Serial.print(" curr=");
        Serial.println((uintptr_t)currFrame, HEX);
        if (!prevFrame || !currFrame) {
          Serial.println("Failed to allocate camera frame buffers - cleaning up");
          if (prevFrame) { free(prevFrame); prevFrame = NULL; }
          if (currFrame) { free(currFrame); currFrame = NULL; }
          // Deinit camera to avoid driver using memory when we can't allocate buffers
          // esp_camera_deinit() may not be available in all SDK versions — skip if undefined
          // (no-op)
          cameraAvailable = false;
        } else {
          memset(prevFrame, 0, CAM_FRAME_WIDTH * CAM_FRAME_HEIGHT);
          memset(currFrame, 0, CAM_FRAME_WIDTH * CAM_FRAME_HEIGHT);
          Serial.println("Camera frame buffers allocated and cleared");
        }
      } else {
        Serial.println("Camera initialization failed — cameraAvailable=false");
      }
      break;
    }
  }
}

SensorReadings readSensor() {
  SensorReadings reading = {0.0, 0.0, 0.0, false};
  
  switch (SENSOR) {
    case SENSOR_DHT_TEMP: {
      sensors.requestTemperatures();
      float temp = sensors.getTempCByIndex(0);
      
      if (temp != DEVICE_DISCONNECTED_C) {
        reading.temperature = temp;
        reading.humidity = 0.0;  // Placeholder - would need DHT library for actual humidity
        reading.success = true;
      } else {
        Serial.println("Temperature sensor error!");
        reading.success = false;
      }
      break;
    }
    
    case SENSOR_MQ7_GAS: {
      // Read MQ7 analog value
      int rawValue = analogRead(MQ7_ANALOG_PIN);
      bool digitalThreshold = digitalRead(MQ7_DIGITAL_PIN);
      
      // Convert ADC value to voltage
      float voltage = (rawValue / MQ7_ADC_MAX) * MQ7_VCC;
      
      // Calculate Rs (sensor resistance) from voltage divider
      // Vout = VCC * RL / (RS + RL)
      // RS = RL * (VCC / Vout - 1)
      float rs = MQ7_RL * (MQ7_VCC / voltage - 1.0);
      
      // Calculate Rs/R0 ratio
      float ratio = rs / MQ7_RO;
      
      // Convert ratio to ppm using MQ7 response curve
      // ppm = (Rs/R0 / a) ^ (1/b)
      float ppm = 0.0;
      if (ratio > 0) {
        ppm = CO_CURVE_A * pow(ratio, CO_CURVE_B);
        if (ppm < 0) ppm = 0;  // Ensure non-negative
      }
      
      reading.gasLevel = ppm;
      reading.digitalThreshold = digitalThreshold;
      reading.success = true;
      
      // Categorize gas level based on ppm thresholds
      if (ppm < 5.0) {
        reading.gasCategoryLevel = GAS_LOW;
      } else if (ppm < 15.0) {
        reading.gasCategoryLevel = GAS_MEDIUM;
      } else if (ppm < 35.0) {
        reading.gasCategoryLevel = GAS_HIGH;
      } else {
        reading.gasCategoryLevel = GAS_CRITICAL;
      }
      
      // Update global gas level for LED status
      currentGasLevel = reading.gasCategoryLevel;
      
      // Debug output
      Serial.print("MQ7 - ADC Raw: ");
      Serial.print(rawValue);
      Serial.print(" | Voltage: ");
      Serial.print(voltage, 2);
      Serial.print("V | Rs: ");
      Serial.print(rs, 1);
      Serial.print("kΩ | Ratio: ");
      Serial.print(ratio, 3);
      Serial.print(" | CO Level: ");
      Serial.print(ppm, 1);
      Serial.print(" ppm (");
      switch(reading.gasCategoryLevel) {
        case GAS_LOW: Serial.print("LOW"); break;
        case GAS_MEDIUM: Serial.print("MEDIUM"); break;
        case GAS_HIGH: Serial.print("HIGH"); break;
        case GAS_CRITICAL: Serial.print("CRITICAL"); break;
      }
      Serial.print(") | Digital Threshold: ");
      Serial.println(digitalThreshold ? "HIGH" : "LOW");
      break;
    }
  }
  
  return reading;
}

void publishSensorData() {
  // This is a convenience wrapper for readAndPublishTemperature
  // Called automatically in the main loop
  readAndPublishTemperature();
}

// Update and publish events for the SENSOR_SENSE doorway tracker
void updateSenseSensor() {
  unsigned long now = millis();

  if (SENSOR == SENSOR_SENSE) {
    bool aActive = (digitalRead(SENSE_A_PIN) == (SENSE_ACTIVE_LOW ? LOW : HIGH));
    bool bActive = (digitalRead(SENSE_B_PIN) == (SENSE_ACTIVE_LOW ? LOW : HIGH));

    switch (senseState) {
      case SENSE_IDLE:
        if (aActive && !bActive) {
          senseState = SENSE_A_TRIGGERED;
          senseLastTriggerTime = now;
        } else if (bActive && !aActive) {
          senseState = SENSE_B_TRIGGERED;
          senseLastTriggerTime = now;
        }
        break;

      case SENSE_A_TRIGGERED:
        // Wait for B to trigger within timeout => entry (A -> B)
        if (bActive) {
          senseInCount++;
          publishTrafficEvent("enter", senseInCount, senseOutCount);
          senseState = SENSE_IDLE;
        } else if (now - senseLastTriggerTime > SENSE_TIMEOUT_MS) {
          // Timeout - reset
          senseState = SENSE_IDLE;
        }
        break;

      case SENSE_B_TRIGGERED:
        // Wait for A to trigger within timeout => exit (B -> A)
        if (aActive) {
          senseOutCount++;
          publishTrafficEvent("exit", senseInCount, senseOutCount);
          senseState = SENSE_IDLE;
        } else if (now - senseLastTriggerTime > SENSE_TIMEOUT_MS) {
          // Timeout - reset
          senseState = SENSE_IDLE;
        }
        break;
    }
  } else if (SENSOR == SENSOR_SENSE_CAMERA) {
    // Camera-based processing
    if (cameraAvailable) {
      processCameraFrame();
    }
  }
}

void publishTrafficEvent(const char* event, int inCount, int outCount) {
  Serial.print("Traffic event: "); Serial.println(event);
  Serial.print("Counts - In: "); Serial.print(inCount); Serial.print(" Out: "); Serial.println(outCount);

  JsonDocument doc;
  doc["device_id"] = DEVICE_ID;
  doc["timestamp"] = getCurrentUnixTime();
  doc["event"] = event;
  doc["in_count"] = inCount;
  doc["out_count"] = outCount;

  String jsonString;
  serializeJson(doc, jsonString);

  String sensorTopic = deviceHostname + "/sensors/traffic";
  if (mqttClient.connected()) {
    setLEDStatus(LED_MQTT_SENDING);
    lastMQTTSend = millis();
    mqttClient.publish(sensorTopic.c_str(), jsonString.c_str());
    Serial.print("Published traffic event to: "); Serial.println(sensorTopic);
  } else {
    Serial.println("MQTT not connected - skipping publish");
  }
}

// -------- Camera helper implementations --------
#ifdef ESP32
#include "esp_camera.h"

bool initializeCameraModule() {
  Serial.print("Camera init: free heap before init: "); Serial.println(ESP.getFreeHeap());

  // Require some free heap for camera driver and frame buffers
  const size_t MIN_HEAP_FOR_CAMERA = 150000; // ~150KB
  if (ESP.getFreeHeap() < MIN_HEAP_FOR_CAMERA) {
    Serial.println("Insufficient heap for camera init, skipping camera");
    return false;
  }

  camera_config_t config;
  memset(&config, 0, sizeof(config));
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = CAM_PIN_D0;
  config.pin_d1 = CAM_PIN_D1;
  config.pin_d2 = CAM_PIN_D2;
  config.pin_d3 = CAM_PIN_D3;
  config.pin_d4 = CAM_PIN_D4;
  config.pin_d5 = CAM_PIN_D5;
  config.pin_d6 = CAM_PIN_D6;
  config.pin_d7 = CAM_PIN_D7;
  config.pin_xclk = CAM_PIN_XCLK;
  config.pin_pclk = CAM_PIN_PCLK;
  config.pin_vsync = CAM_PIN_VSYNC;
  config.pin_href = CAM_PIN_HREF;
  config.pin_sccb_sda = CAM_PIN_SIOD;
  config.pin_sccb_scl = CAM_PIN_SIOC;
  config.pin_pwdn = CAM_PIN_PWDN;
  config.pin_reset = CAM_PIN_RESET;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_RGB565;
  config.frame_size = FRAMESIZE_QQVGA; // 160x120
  config.jpeg_quality = 12;
  config.fb_count = 1;

  // If a previous camera driver is loaded, deinit first to avoid conflicts
  // esp_camera_deinit() may not be available in this SDK; skip explicit deinit

  Serial.println("Attempting esp_camera_init()...");
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.print("Camera init failed with error 0x"); Serial.println(err, HEX);
    Serial.print("Free heap after failed init: "); Serial.println(ESP.getFreeHeap());
    return false;
  }
  Serial.print("Camera initialized successfully; free heap after init: ");
  Serial.println(ESP.getFreeHeap());

  // After init, perform gentle sensor tuning if available
  sensor_t* s = esp_camera_sensor_get();
  if (s) {
    // Set frame size for RGB565 working mode used for frame-diff
    s->set_framesize(s, FRAMESIZE_QQVGA);
    // Optionally increase contrast/brightness/gain for better diff results
    // s->set_contrast(s, 1);
    // s->set_brightness(s, 1);
    Serial.println("Camera sensor tuning applied: framesize=QQVGA");
  }

  // Try a quick test capture to validate the camera is responsive.
  // Some boards/drivers return success from esp_camera_init but fail to allocate
  // or capture frames until a short delay or retry.
  delay(200);
  camera_fb_t* test_fb = NULL;
  for (int i = 0; i < 3; i++) {
    test_fb = esp_camera_fb_get();
    if (test_fb) break;
    Serial.print("Test capture attempt "); Serial.print(i+1); Serial.println(" failed, retrying...");
    delay(200);
  }

  if (!test_fb) {
    Serial.println("Camera test capture failed after init; deinitializing camera");
    Serial.print("Free heap: "); Serial.println(ESP.getFreeHeap());
    // esp_camera_deinit may not exist; attempt to free any internal resources by calling init with fb_count=0
    return false;
  }

  Serial.print("Test capture OK: format="); Serial.print(test_fb->format);
  Serial.print(" size="); Serial.println(test_fb->len);
  esp_camera_fb_return(test_fb);
  return true;
}

// Capture a grayscale frame resized to w*h. If JPEG capture is returned,
// we decode to grayscale by simple sampling of luminance from JPEG buffer —
// for simplicity we request PIXFORMAT_RGB565 when supported; fallback to
// converting JPEG to grayscale is expensive and not implemented fully here.
bool captureGrayscaleFrame(uint8_t* buf, int w, int h) {
  if (!buf) return false;
  camera_fb_t* fb = NULL;
  // Retry a few times — sometimes the driver returns NULL briefly
  for (int attempt = 0; attempt < 4; attempt++) {
    fb = esp_camera_fb_get();
    if (fb) break;
    Serial.print("Camera capture attempt "); Serial.print(attempt+1); Serial.println(" failed, retrying...");
    delay(100);
  }
  if (!fb) {
    Serial.print("Camera capture failed; free heap: "); Serial.println(ESP.getFreeHeap());
    // Try a recovery: deinit and reinit the camera once, then attempt capture again
    Serial.println("Attempting camera recovery: deinit + reinit...");
    // esp_camera_deinit() may return error on some SDKs; log but continue
    esp_err_t derr = esp_camera_deinit();
    Serial.print("esp_camera_deinit() returned: "); Serial.println(derr);
    delay(100);
    bool reinited = initializeCameraModule();
    if (!reinited) {
      Serial.println("Camera recovery failed (reinit unsuccessful)");
      return false;
    }

    // Try capturing again after recovery
    for (int attempt2 = 0; attempt2 < 4; attempt2++) {
      fb = esp_camera_fb_get();
      if (fb) break;
      Serial.print("Recovery capture attempt "); Serial.print(attempt2+1); Serial.println(" failed, retrying...");
      delay(100);
    }
    if (!fb) {
      Serial.println("Camera capture failed after recovery");
      return false;
    }
  }

  // If framebuffer is in RGB565 format, convert to grayscale easily
  if (fb->format == PIXFORMAT_RGB565 && fb->width >= w && fb->height >= h) {
    // Simple nearest-neighbor downsample
    for (int y = 0; y < h; y++) {
      for (int x = 0; x < w; x++) {
        int sx = x * fb->width / w;
        int sy = y * fb->height / h;
        int idx = sy * fb->width + sx;
        uint16_t pix = ((uint16_t*)fb->buf)[idx];
        // RGB565 -> approximate grayscale
        uint8_t r = ((pix >> 11) & 0x1F) << 3;
        uint8_t g = ((pix >> 5) & 0x3F) << 2;
        uint8_t b = (pix & 0x1F) << 3;
        uint8_t gray = (uint8_t)((0.3 * r) + (0.59 * g) + (0.11 * b));
        buf[y * w + x] = gray;
      }
    }
    esp_camera_fb_return(fb);
    return true;
  }

  // If JPEG, we can try to extract luminance by rough sampling — heavy.
  // For now, we fail gracefully and inform the caller.
  Serial.println("captureGrayscaleFrame: unsupported pixel format (need RGB565) — try changing camera config");
  esp_camera_fb_return(fb);
  return false;
}

// Upload raw frame buffer to configured HTTP endpoint using multipart/form-data
// Upload raw frame buffer to configured HTTP endpoint. Returns HTTP response body on success, empty string on failure.
String uploadFrameToServer(const uint8_t* data, size_t len, const char* filename, const char* mimetype) {
  if (CAMERA_FRAME_UPLOAD_METHOD == UPLOAD_NONE) return String("");
  if (!wifiConnected) {
    Serial.println("Skipping HTTP upload: WiFi not connected");
    return String("");
  }

  HTTPClient http;
  String responseBody = "";

  if (CAMERA_FRAME_UPLOAD_METHOD == UPLOAD_HTTP_JWT) {
    if (strlen(CAMERA_FRAME_HTTP_ENDPOINT) == 0) return String("");
    String jwt = generateJWT();
    http.begin(CAMERA_FRAME_HTTP_ENDPOINT);
    http.addHeader("Authorization", "Bearer " + jwt);

    String boundary = "----ESP32CAM" + String(millis());
    String contentType = "multipart/form-data; boundary=" + boundary;
    http.addHeader("Content-Type", contentType);

    String pre = "--" + boundary + "\r\n";
    pre += String("Content-Disposition: form-data; name=\"file\"; filename=\"") + filename + "\"\r\n";
    pre += String("Content-Type: ") + mimetype + "\r\n\r\n";
    String post = "\r\n--" + boundary + "--\r\n";

    size_t preLen = pre.length();
    size_t postLen = post.length();
    size_t total = preLen + len + postLen;

    uint8_t* body = (uint8_t*)malloc(total);
    if (!body) {
      Serial.println("Failed to allocate memory for multipart body");
      http.end();
      return String("");
    }

    memcpy(body, pre.c_str(), preLen);
    memcpy(body + preLen, data, len);
    memcpy(body + preLen + len, post.c_str(), postLen);

    Serial.print("HTTP upload (JWT multipart): posting "); Serial.print(total); Serial.println(" bytes");
    int httpCode = http.sendRequest("POST", body, total);
    if (httpCode > 0) {
      Serial.print("HTTP response code: "); Serial.println(httpCode);
      responseBody = http.getString();
      Serial.print("HTTP response body: "); Serial.println(responseBody);
    } else {
      Serial.print("HTTP upload failed, error: "); Serial.println(httpCode);
    }
    free(body);
    http.end();
    if (httpCode >= 200 && httpCode < 300) return responseBody;
    return String("");
  }

  // NTFY mode: POST raw bytes directly to endpoint (no multipart)
  if (CAMERA_FRAME_UPLOAD_METHOD == UPLOAD_NTFY) {
    if (strlen(CAMERA_FRAME_HTTP_ENDPOINT) == 0) return String("");
    http.begin(CAMERA_FRAME_HTTP_ENDPOINT);
    if (strlen(CAMERA_NTFY_TOKEN) > 0) {
      http.addHeader("Authorization", "Bearer " + String(CAMERA_NTFY_TOKEN));
    }
    http.addHeader("Content-Type", mimetype);
    String disp = String("attachment; filename=\"") + String(filename) + "\"";
    http.addHeader("Content-Disposition", disp);
    http.addHeader("Title", String("Frame from ") + deviceHostname);

    Serial.print("HTTP upload (ntfy): posting "); Serial.print(len); Serial.println(" bytes");
    int httpCode = http.sendRequest("POST", (uint8_t*)data, len);
    if (httpCode > 0) {
      Serial.print("NTFY response code: "); Serial.println(httpCode);
      responseBody = http.getString();
      Serial.print("NTFY response body: "); Serial.println(responseBody);
    } else {
      Serial.print("NTFY upload failed, error: "); Serial.println(httpCode);
    }
    http.end();
    if (httpCode >= 200 && httpCode < 300) return responseBody;
    return String("");
  }

  return String("");
}

void processCameraFrame() {
  // Capture into currFrame
  if (!captureGrayscaleFrame(currFrame, CAM_FRAME_WIDTH, CAM_FRAME_HEIGHT)) {
    return;
  }

  // Compute per-column motion energy by summing difference across rows
  int width = CAM_FRAME_WIDTH;
  int height = CAM_FRAME_HEIGHT;
  int centerX = width / 2;

  int maxColSum = 0;
  int maxColIndex = -1;
  int totalMotionPixels = 0;

  for (int x = 0; x < width; x++) {
    int colSum = 0;
    for (int y = 0; y < height; y++) {
      int idx = y * width + x;
      int diff = abs((int)currFrame[idx] - (int)prevFrame[idx]);
      if (diff > CAM_DIFF_THRESHOLD) {
        colSum++;
        totalMotionPixels++;
      }
    }
    if (colSum > maxColSum) {
      maxColSum = colSum;
      maxColIndex = x;
    }
  }

  // Shift current to prev for next iteration
  memcpy(prevFrame, currFrame, width * height);

  // If insufficient motion, ignore
  if (totalMotionPixels < CAM_MOTION_MIN_PIXELS) {
    prevMotionX = -1;
    return;
  }

  Serial.print("Camera motion pixels: "); Serial.print(totalMotionPixels);
  Serial.print(" | maxColIndex: "); Serial.print(maxColIndex);
  Serial.print(" | prevMotionX: "); Serial.println(prevMotionX);

  // Determine crossing based on previous motion X
  if (prevMotionX >= 0) {
    if (prevMotionX < centerX && maxColIndex >= centerX) {
      // left->right crossing => enter
      senseInCount++;
      publishTrafficEvent("enter", senseInCount, senseOutCount);
    } else if (prevMotionX > centerX && maxColIndex <= centerX) {
      // right->left crossing => exit
      senseOutCount++;
      publishTrafficEvent("exit", senseInCount, senseOutCount);
    } else {
      // No clear crossing; if motion very large, treat as ambiguous
      if (totalMotionPixels > CAM_AMBIGUOUS_PIXELS) {
        // Capture a full JPEG frame and publish to MQTT for server-side processing
        // Try to capture a JPEG frame (temporarily reconfigure camera to JPEG)
        Serial.println("Ambiguous motion detected - attempting JPEG capture for upload");
        camera_fb_t* fb = NULL;
        // First try to get whatever frame driver currently provides
        fb = esp_camera_fb_get();
        if (fb && fb->format == PIXFORMAT_JPEG) {
          // Already JPEG - use it directly
          Serial.print("Captured ambiguous JPEG frame, size="); Serial.println(fb->len);
          String topic = deviceHostname + "/" + String(CAMERA_FRAME_TOPIC_BASE);
          if (mqttClient.connected()) {
            // Publish metadata first
            JsonDocument meta;
            meta["device_id"] = DEVICE_ID;
            meta["timestamp"] = getCurrentUnixTime();
            meta["event"] = "ambiguous_frame";
            meta["size"] = fb->len;
            meta["format"] = "jpeg";
            String metaStr;
            serializeJson(meta, metaStr);
            String metaTopic = deviceHostname + "/" + String(CAMERA_FRAME_TOPIC_BASE) + "/meta";
            mqttClient.publish(metaTopic.c_str(), metaStr.c_str());
            // Then publish binary
            String binTopic = deviceHostname + "/" + String(CAMERA_FRAME_TOPIC_BASE) + "/bin";
            mqttClient.publish(binTopic.c_str(), (const uint8_t*)fb->buf, fb->len);
            Serial.print("Published ambiguous frame to MQTT topic: "); Serial.println(binTopic);
          }
          if (strlen(CAMERA_FRAME_HTTP_ENDPOINT) > 0) {
            String resp = uploadFrameToServer((const uint8_t*)fb->buf, fb->len, "frame.jpg", "image/jpeg");
            Serial.print("HTTP upload response: "); Serial.println(resp);
            if (resp.length() > 0 && mqttClient.connected()) {
              String urlTopic = deviceHostname + "/" + String(CAMERA_FRAME_TOPIC_BASE) + "/url";
              mqttClient.publish(urlTopic.c_str(), resp.c_str());
            }
          }
          esp_camera_fb_return(fb);
        } else {
          if (fb) { esp_camera_fb_return(fb); fb = NULL; }
          // Attempt to re-init camera in JPEG mode, capture one frame, then restore RGB565
          Serial.println("Reinitializing camera to JPEG mode for capture...");
          // Deinitialize camera first to avoid invalid-state errors
          Serial.println("Calling esp_camera_deinit() before JPEG reinit...");
          esp_err_t deinit_r = esp_camera_deinit();
          Serial.print("esp_camera_deinit() returned: "); Serial.println(deinit_r);
          delay(100);

          camera_config_t jpegConfig;
          memset(&jpegConfig, 0, sizeof(jpegConfig));
          jpegConfig.ledc_channel = LEDC_CHANNEL_0;
          jpegConfig.ledc_timer = LEDC_TIMER_0;
          jpegConfig.pin_d0 = CAM_PIN_D0;
          jpegConfig.pin_d1 = CAM_PIN_D1;
          jpegConfig.pin_d2 = CAM_PIN_D2;
          jpegConfig.pin_d3 = CAM_PIN_D3;
          jpegConfig.pin_d4 = CAM_PIN_D4;
          jpegConfig.pin_d5 = CAM_PIN_D5;
          jpegConfig.pin_d6 = CAM_PIN_D6;
          jpegConfig.pin_d7 = CAM_PIN_D7;
          jpegConfig.pin_xclk = CAM_PIN_XCLK;
          jpegConfig.pin_pclk = CAM_PIN_PCLK;
          jpegConfig.pin_vsync = CAM_PIN_VSYNC;
          jpegConfig.pin_href = CAM_PIN_HREF;
          jpegConfig.pin_sccb_sda = CAM_PIN_SIOD;
          jpegConfig.pin_sccb_scl = CAM_PIN_SIOC;
          jpegConfig.pin_pwdn = CAM_PIN_PWDN;
          jpegConfig.pin_reset = CAM_PIN_RESET;
          jpegConfig.xclk_freq_hz = CAMERA_JPEG_XCLK_HZ;
          jpegConfig.pixel_format = PIXFORMAT_JPEG;
          jpegConfig.frame_size = CAMERA_JPEG_FRAME_SIZE;
          jpegConfig.jpeg_quality = CAMERA_JPEG_QUALITY;
          jpegConfig.fb_count = 1;

          // Try to init JPEG mode (try once, log error on failure)
          esp_err_t r = esp_camera_init(&jpegConfig);
          if (r == ESP_OK) {
            camera_fb_t* jfb = esp_camera_fb_get();
            if (jfb) {
              Serial.print("Captured JPEG frame after reinit, size="); Serial.println(jfb->len);
              if (mqttClient.connected()) {
                // Publish metadata first
                JsonDocument meta;
                meta["device_id"] = DEVICE_ID;
                meta["timestamp"] = getCurrentUnixTime();
                meta["event"] = "ambiguous_frame";
                meta["size"] = jfb->len;
                meta["format"] = "jpeg";
                String metaStr;
                serializeJson(meta, metaStr);
                String metaTopic = deviceHostname + "/" + String(CAMERA_FRAME_TOPIC_BASE) + "/meta";
                mqttClient.publish(metaTopic.c_str(), metaStr.c_str());

                // Then publish binary
                String binTopic = deviceHostname + "/" + String(CAMERA_FRAME_TOPIC_BASE) + "/bin";
                mqttClient.publish(binTopic.c_str(), (const uint8_t*)jfb->buf, jfb->len);
              }
              if (strlen(CAMERA_FRAME_HTTP_ENDPOINT) > 0) {
                String resp = uploadFrameToServer((const uint8_t*)jfb->buf, jfb->len, "frame.jpg", "image/jpeg");
                Serial.print("HTTP upload response: "); Serial.println(resp);
                if (resp.length() > 0 && mqttClient.connected()) {
                  String urlTopic = deviceHostname + "/" + String(CAMERA_FRAME_TOPIC_BASE) + "/url";
                  mqttClient.publish(urlTopic.c_str(), resp.c_str());
                }
              }
              esp_camera_fb_return(jfb);
            } else {
              Serial.println("Failed to capture JPEG frame after reinit");
            }
            // Attempt to restore RGB565 mode for continued frame-diff processing
            Serial.println("Restoring camera to RGB565 mode...");
            // Deinit before restoring
            esp_err_t deinit2 = esp_camera_deinit();
            Serial.print("esp_camera_deinit() returned (before restoring RGB): "); Serial.println(deinit2);
            delay(100);
            camera_config_t rgbConfig;
            memset(&rgbConfig, 0, sizeof(rgbConfig));
            rgbConfig.ledc_channel = LEDC_CHANNEL_0;
            rgbConfig.ledc_timer = LEDC_TIMER_0;
            rgbConfig.pin_d0 = CAM_PIN_D0;
            rgbConfig.pin_d1 = CAM_PIN_D1;
            rgbConfig.pin_d2 = CAM_PIN_D2;
            rgbConfig.pin_d3 = CAM_PIN_D3;
            rgbConfig.pin_d4 = CAM_PIN_D4;
            rgbConfig.pin_d5 = CAM_PIN_D5;
            rgbConfig.pin_d6 = CAM_PIN_D6;
            rgbConfig.pin_d7 = CAM_PIN_D7;
            rgbConfig.pin_xclk = CAM_PIN_XCLK;
            rgbConfig.pin_pclk = CAM_PIN_PCLK;
            rgbConfig.pin_vsync = CAM_PIN_VSYNC;
            rgbConfig.pin_href = CAM_PIN_HREF;
            rgbConfig.pin_sccb_sda = CAM_PIN_SIOD;
            rgbConfig.pin_sccb_scl = CAM_PIN_SIOC;
            rgbConfig.pin_pwdn = CAM_PIN_PWDN;
            rgbConfig.pin_reset = CAM_PIN_RESET;
            rgbConfig.xclk_freq_hz = 20000000;
            rgbConfig.pixel_format = PIXFORMAT_RGB565;
            rgbConfig.frame_size = FRAMESIZE_QQVGA;
            rgbConfig.jpeg_quality = 12;
            rgbConfig.fb_count = 1;
            esp_err_t r2 = esp_camera_init(&rgbConfig);
            if (r2 != ESP_OK) {
              Serial.print("Failed to restore RGB565 camera mode: "); Serial.println(r2);
              cameraAvailable = false;
            }
          } else {
            Serial.print("Failed to reinit camera to JPEG mode: "); Serial.println(r);
          }
        }
      }
    }
  }

  prevMotionX = maxColIndex;
}
#else
bool initializeCameraModule() { Serial.println("Camera not supported on this platform"); return false; }
bool captureGrayscaleFrame(uint8_t* buf, int w, int h) { (void)buf; (void)w; (void)h; return false; }
void processCameraFrame() { }
#endif

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

void updateAttachments() {
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
      attachmentSetColor(CRGB::Black, 0);
      break;

    case LED_WIFI_CONNECTING:
      attachmentBlink(CRGB::Blue, 1000); // Slow blue blink
      break;

    case LED_MQTT_CONNECTING:
      attachmentBlink(CRGB::Yellow, 250); // Fast yellow blink
      break;

    case LED_MQTT_CONNECTED:
      // For MQ7 sensor, show air quality status with breathing LED
      if (SENSOR == SENSOR_MQ7_GAS) {
        switch (currentGasLevel) {
          case GAS_LOW:
            attachmentBreathe(CRGB::Green, 5000);  // Green = safe
            break;
          case GAS_MEDIUM:
            attachmentBreathe(CRGB::Yellow, 5000); // Yellow = caution
            break;
          case GAS_HIGH:
            attachmentBreathe(CRGB(255, 165, 0), 5000); // Orange = warning
            break;
          case GAS_CRITICAL:
            attachmentBlink(CRGB::Red, 250);  // Red blink = danger
            break;
        }
      } else {
        // For temperature sensor, just green breathing
        attachmentBreathe(CRGB::Green, 5000);
      }
      break;

    case LED_MQTT_SENDING:
      attachmentFlash(CRGB::White, 150); // Brief white flash
      break;

    case LED_MQTT_RECEIVING:
      attachmentFlash(CRGB::Red, 150); // Brief red flash
      break;

    case LED_OTA_DOWNLOADING:
      // This will be updated with actual progress in OTA functions
      attachmentSpinning(CRGB::Orange, 100); // Spinning (if supported)
      break;

    case LED_OTA_VALIDATING:
      attachmentSpinning(CRGB::Cyan, 50); // Fast spin
      break;

    case LED_OTA_INSTALLING:
      attachmentPulse(CRGB::Red, 200); // Fast red pulse
      break;

    case LED_TEMPERATURE_READ:
      attachmentTemperatureFlash(); // Brief white flash
      break;

    case LED_ERROR:
      attachmentBlink(CRGB::Red, 200); // Fast red blink
      break;
  }
}
