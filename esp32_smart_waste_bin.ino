
#define BLYNK_TEMPLATE_ID "YourTemplateID"
#define BLYNK_DEVICE_NAME "Smart Waste Bin"
#define BLYNK_AUTH_TOKEN "YourAuthToken"

#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <ESP32Servo.h>
#include <ArduinoJson.h>
#include <WebServer.h>
#include "esp_camera.h"
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

// WiFi credentials
char ssid[] = "YourWiFiSSID";
char pass[] = "YourWiFiPassword";

// Pin definitions for ESP32 (38 pins) with external OV2640 camera
#define PWDN_GPIO_NUM     -1  // Not used with external module
#define RESET_GPIO_NUM    -1  // Not used with external module
#define XCLK_GPIO_NUM      4  // External clock
#define SIOD_GPIO_NUM     18  // SDA
#define SIOC_GPIO_NUM     19  // SCL
#define Y9_GPIO_NUM       36  // D7
#define Y8_GPIO_NUM       39  // D6
#define Y7_GPIO_NUM       34  // D5
#define Y6_GPIO_NUM       35  // D4
#define Y5_GPIO_NUM       32  // D3
#define Y4_GPIO_NUM       33  // D2
#define Y3_GPIO_NUM       25  // D1
#define Y2_GPIO_NUM       26  // D0
#define VSYNC_GPIO_NUM    27  // VSYNC
#define HREF_GPIO_NUM     23  // HREF
#define PCLK_GPIO_NUM     22  // PCLK

// Sensor and actuator pins
#define SERVO_PIN         13
#define AIR_QUALITY_PIN   A0   // GPIO36 (ADC1_0) - MQ135 sensor
#define BUZZER_PIN        12
#define LED_GREEN         14
#define LED_RED           2
#define ULTRASONIC_TRIG   5
#define ULTRASONIC_ECHO   17

// Servo positions
#define SERVO_CENTER      90
#define SERVO_LEFT        45   // Biodegradable
#define SERVO_RIGHT       135  // Non-biodegradable

Servo wasteServo;
WebServer server(80);

// Sensor variables
int airQualityThreshold = 800;
int currentAirQuality = 0;
bool alertSent = false;
bool objectDetected = false;
unsigned long lastDetectionTime = 0;
const unsigned long detectionDelay = 2000; // 2 seconds

// Blynk virtual pins
#define V0 0  // Air quality value
#define V1 1  // Waste classification result
#define V2 2  // Servo position
#define V3 3  // Alert status
#define V4 4  // Camera trigger
#define V5 5  // Object detection status

void setup() {
  Serial.begin(115200);
  
  // Disable brownout detector
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
  
  // Initialize pins
  pinMode(AIR_QUALITY_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_RED, OUTPUT);
  pinMode(ULTRASONIC_TRIG, OUTPUT);
  pinMode(ULTRASONIC_ECHO, INPUT);
  
  // Initialize servo
  wasteServo.attach(SERVO_PIN);
  wasteServo.write(SERVO_CENTER);
  
  // Initialize camera configuration for external OV2640
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = FRAMESIZE_SVGA; // SVGA for external camera
  config.jpeg_quality = 12;
  config.fb_count = 1;
  
  // Initialize camera
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x\n", err);
    Serial.println("Operating without camera...");
  } else {
    Serial.println("Camera initialized successfully");
  }
  
  // Connect to WiFi and Blynk
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
  
  // Setup web server routes
  server.on("/", handleRoot);
  server.on("/capture", handleCapture);
  server.on("/servo/left", handleServoLeft);
  server.on("/servo/right", handleServoRight);
  server.on("/servo/center", handleServoCenter);
  server.on("/air-quality", handleAirQuality);
  server.on("/distance", handleDistanceCheck);
  server.on("/status", handleStatus);
  server.begin();
  
  // Initial LED indication
  digitalWrite(LED_GREEN, HIGH);
  delay(1000);
  digitalWrite(LED_GREEN, LOW);
  
  Serial.println("Smart Waste Bin System Ready!");
  Serial.println("Pin Configuration:");
  Serial.println("- Servo: GPIO" + String(SERVO_PIN));
  Serial.println("- Air Quality: GPIO" + String(AIR_QUALITY_PIN));
  Serial.println("- Buzzer: GPIO" + String(BUZZER_PIN));
  Serial.println("- Green LED: GPIO" + String(LED_GREEN));
  Serial.println("- Red LED: GPIO" + String(LED_RED));
  Serial.println("- Ultrasonic Trig: GPIO" + String(ULTRASONIC_TRIG));
  Serial.println("- Ultrasonic Echo: GPIO" + String(ULTRASONIC_ECHO));
}

void loop() {
  Blynk.run();
  server.handleClient();
  
  // Check for object detection using ultrasonic sensor
  float distance = getDistance();
  bool currentDetection = (distance > 0 && distance < 15); // 15cm threshold
  
  if (currentDetection && !objectDetected) {
    objectDetected = true;
    lastDetectionTime = millis();
    Serial.println("Object detected! Distance: " + String(distance) + "cm");
    Blynk.virtualWrite(V5, "Object Detected");
    digitalWrite(LED_GREEN, LOW);
    digitalWrite(LED_RED, HIGH);
    
    // Trigger automatic capture and classification after delay
    delay(detectionDelay);
    if (getDistance() < 15) { // Confirm object is still there
      captureAndClassify();
    }
  } else if (!currentDetection && objectDetected) {
    objectDetected = false;
    Serial.println("Object removed");
    Blynk.virtualWrite(V5, "Ready");
    resetToCenter();
  }
  
  // Read air quality sensor
  currentAirQuality = analogRead(AIR_QUALITY_PIN);
  currentAirQuality = map(currentAirQuality, 0, 4095, 0, 1000); // Convert to approximate ppm
  
  // Send air quality to Blynk
  Blynk.virtualWrite(V0, currentAirQuality);
  
  // Check air quality threshold
  if (currentAirQuality > airQualityThreshold && !alertSent) {
    sendAlert();
    alertSent = true;
  } else if (currentAirQuality <= (airQualityThreshold - 50)) { // Hysteresis
    alertSent = false;
  }
  
  // Update LED status based on air quality (when no object detected)
  if (!objectDetected) {
    if (currentAirQuality > airQualityThreshold) {
      digitalWrite(LED_RED, HIGH);
      digitalWrite(LED_GREEN, LOW);
    } else {
      digitalWrite(LED_RED, LOW);
      digitalWrite(LED_GREEN, HIGH);
    }
  }
  
  delay(1000); // Update every second
}

float getDistance() {
  digitalWrite(ULTRASONIC_TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(ULTRASONIC_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(ULTRASONIC_TRIG, LOW);
  
  long duration = pulseIn(ULTRASONIC_ECHO, HIGH, 30000); // 30ms timeout
  if (duration == 0) return -1; // No echo
  
  float distance = (duration * 0.034) / 2; // Convert to cm
  return distance;
}

// Blynk virtual pin handlers
BLYNK_WRITE(V2) {
  int servoPos = param.asInt();
  wasteServo.write(servoPos);
  Serial.println("Servo position set via Blynk: " + String(servoPos));
}

BLYNK_WRITE(V4) {
  if (param.asInt() == 1) {
    Serial.println("Manual capture triggered via Blynk");
    captureAndSend();
  }
}

// Web server handlers
void handleRoot() {
  String html = "<!DOCTYPE html><html><head><title>Smart Waste Bin</title>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<style>body{font-family:Arial;margin:20px;} .btn{display:block;margin:10px 0;padding:10px;background:#4CAF50;color:white;text-decoration:none;border-radius:5px;text-align:center;} .status{background:#f0f0f0;padding:10px;margin:10px 0;border-radius:5px;}</style>";
  html += "</head><body>";
  html += "<h1>Smart Waste Bin Control Panel</h1>";
  html += "<div class='status'><strong>System Status</strong><br>";
  html += "Air Quality: " + String(currentAirQuality) + " ppm<br>";
  html += "Distance: " + String(getDistance()) + " cm<br>";
  html += "Object: " + String(objectDetected ? "Detected" : "None") + "</div>";
  html += "<a href='/capture' class='btn'>📷 Capture Image</a>";
  html += "<a href='/servo/left' class='btn'>⬅️ Biodegradable (Left)</a>";
  html += "<a href='/servo/right' class='btn'>➡️ Non-biodegradable (Right)</a>";
  html += "<a href='/servo/center' class='btn'>⏺️ Center Position</a>";
  html += "<a href='/air-quality' class='btn'>🌬️ Air Quality JSON</a>";
  html += "<a href='/distance' class='btn'>📏 Distance Check</a>";
  html += "<a href='/status' class='btn'>📊 Full Status</a>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

void handleCapture() {
  camera_fb_t * fb = esp_camera_fb_get();
  if (!fb) {
    server.send(500, "text/plain", "Camera capture failed or not available");
    return;
  }
  
  server.sendHeader("Content-Type", "image/jpeg");
  server.sendHeader("Content-Length", String(fb->len));
  server.sendHeader("Connection", "close");
  server.send_P(200, "image/jpeg", (const char *)fb->buf, fb->len);
  
  esp_camera_fb_return(fb);
  Serial.println("Image captured and sent via web interface");
}

void handleServoLeft() {
  wasteServo.write(SERVO_LEFT);
  Blynk.virtualWrite(V2, SERVO_LEFT);
  Blynk.virtualWrite(V1, "Biodegradable - Left");
  server.send(200, "application/json", "{\"status\":\"moved_left\",\"position\":" + String(SERVO_LEFT) + ",\"classification\":\"biodegradable\"}");
  Serial.println("Servo moved to LEFT (Biodegradable)");
}

void handleServoRight() {
  wasteServo.write(SERVO_RIGHT);
  Blynk.virtualWrite(V2, SERVO_RIGHT);
  Blynk.virtualWrite(V1, "Non-biodegradable - Right");
  server.send(200, "application/json", "{\"status\":\"moved_right\",\"position\":" + String(SERVO_RIGHT) + ",\"classification\":\"non-biodegradable\"}");
  Serial.println("Servo moved to RIGHT (Non-biodegradable)");
}

void handleServoCenter() {
  wasteServo.write(SERVO_CENTER);
  Blynk.virtualWrite(V2, SERVO_CENTER);
  Blynk.virtualWrite(V1, "Centered");
  server.send(200, "application/json", "{\"status\":\"centered\",\"position\":" + String(SERVO_CENTER) + "}");
  Serial.println("Servo moved to CENTER");
}

void handleAirQuality() {
  DynamicJsonDocument doc(1024);
  doc["air_quality"] = currentAirQuality;
  doc["status"] = currentAirQuality > airQualityThreshold ? "alert" : "normal";
  doc["threshold"] = airQualityThreshold;
  doc["alert_sent"] = alertSent;
  
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void handleDistanceCheck() {
  float dist = getDistance();
  DynamicJsonDocument doc(512);
  doc["distance_cm"] = dist;
  doc["object_detected"] = (dist > 0 && dist < 15);
  doc["status"] = objectDetected ? "object_present" : "ready";
  
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void handleStatus() {
  DynamicJsonDocument doc(2048);
  doc["device_name"] = "Smart Waste Bin";
  doc["wifi_status"] = WiFi.status() == WL_CONNECTED ? "connected" : "disconnected";
  doc["ip_address"] = WiFi.localIP().toString();
  doc["air_quality"] = currentAirQuality;
  doc["distance"] = getDistance();
  doc["object_detected"] = objectDetected;
  doc["servo_position"] = wasteServo.read();
  doc["alert_status"] = alertSent;
  doc["uptime_ms"] = millis();
  
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void captureAndSend() {
  camera_fb_t * fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed - no classification possible");
    // Default behavior - assume non-biodegradable for safety
    wasteServo.write(SERVO_RIGHT);
    Blynk.virtualWrite(V1, "No camera - default non-biodegradable");
    return;
  }
  
  Serial.println("Image captured for classification");
  // Here you could send the image to your Python ML server for classification
  // For now, we'll simulate classification
  esp_camera_fb_return(fb);
}

void captureAndClassify() {
  Serial.println("Starting automatic classification...");
  
  // Simulate classification process (replace with actual ML server communication)
  delay(2000); // Simulate processing time
  
  // For demonstration, alternate between classifications
  static bool isEven = false;
  isEven = !isEven;
  
  if (isEven) {
    // Biodegradable
    wasteServo.write(SERVO_LEFT);
    Blynk.virtualWrite(V1, "Biodegradable detected");
    Blynk.virtualWrite(V2, SERVO_LEFT);
    Serial.println("Classification: Biodegradable - moved LEFT");
  } else {
    // Non-biodegradable
    wasteServo.write(SERVO_RIGHT);
    Blynk.virtualWrite(V1, "Non-biodegradable detected");
    Blynk.virtualWrite(V2, SERVO_RIGHT);
    Serial.println("Classification: Non-biodegradable - moved RIGHT");
  }
  
  // Keep position for 3 seconds then return to center
  delay(3000);
}

void resetToCenter() {
  wasteServo.write(SERVO_CENTER);
  Blynk.virtualWrite(V2, SERVO_CENTER);
  Blynk.virtualWrite(V1, "Ready for next item");
  Serial.println("System reset to center position");
}

void sendAlert() {
  // Sound buzzer alert
  for (int i = 0; i < 3; i++) {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(300);
    digitalWrite(BUZZER_PIN, LOW);
    delay(300);
  }
  
  // Send Blynk notification
  Blynk.logEvent("air_quality_alert", "High odor levels detected! Air quality: " + String(currentAirQuality) + " ppm");
  Blynk.virtualWrite(V3, "ALERT: High odor detected! " + String(currentAirQuality) + " ppm");
  
  Serial.println("🚨 ALERT SENT - High odor levels detected! Air quality: " + String(currentAirQuality) + " ppm");
}
</lov-write>
