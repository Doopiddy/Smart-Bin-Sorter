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

// Pin definitions for AI Thinker ESP32-CAM
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

// Servo and sensor pins
#define SERVO_PIN         12
#define AIR_QUALITY_PIN   A0  // MQ135 sensor
#define BUZZER_PIN        14
#define LED_GREEN         15
#define LED_RED           2

// Servo positions
#define SERVO_CENTER      90
#define SERVO_LEFT        45   // Biodegradable
#define SERVO_RIGHT       135  // Non-biodegradable

Servo wasteServo;
WebServer server(80);

// Air quality thresholds
int airQualityThreshold = 800;
int currentAirQuality = 0;
bool alertSent = false;

// Blynk virtual pins
#define V0 0  // Air quality value
#define V1 1  // Waste classification result
#define V2 2  // Servo position
#define V3 3  // Alert status
#define V4 4  // Camera trigger

void setup() {
  Serial.begin(115200);
  
  // Disable brownout detector
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
  
  // Initialize pins
  pinMode(AIR_QUALITY_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_RED, OUTPUT);
  
  // Initialize servo
  wasteServo.attach(SERVO_PIN);
  wasteServo.write(SERVO_CENTER);
  
  // Initialize camera
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
  
  if(psramFound()){
    config.frame_size = FRAMESIZE_UXGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }
  
  // Initialize camera
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
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
  server.begin();
  
  Serial.println("Smart Waste Bin System Ready!");
}

void loop() {
  Blynk.run();
  server.handleClient();
  
  // Read air quality sensor
  currentAirQuality = analogRead(AIR_QUALITY_PIN);
  currentAirQuality = map(currentAirQuality, 0, 4095, 0, 1000); // Convert to ppm
  
  // Send air quality to Blynk
  Blynk.virtualWrite(V0, currentAirQuality);
  
  // Check air quality threshold
  if (currentAirQuality > airQualityThreshold && !alertSent) {
    sendAlert();
    alertSent = true;
    digitalWrite(LED_RED, HIGH);
    digitalWrite(LED_GREEN, LOW);
  } else if (currentAirQuality <= airQualityThreshold) {
    alertSent = false;
    digitalWrite(LED_RED, LOW);
    digitalWrite(LED_GREEN, HIGH);
  }
  
  delay(5000); // Update every 5 seconds
}

// Blynk virtual pin handlers
BLYNK_WRITE(V2) {
  int servoPos = param.asInt();
  wasteServo.write(servoPos);
  Serial.println("Servo position: " + String(servoPos));
}

BLYNK_WRITE(V4) {
  if (param.asInt() == 1) {
    captureAndSend();
  }
}

// Web server handlers
void handleRoot() {
  String html = "<html><body>";
  html += "<h1>Smart Waste Bin Control</h1>";
  html += "<p>Air Quality: " + String(currentAirQuality) + " ppm</p>";
  html += "<a href='/capture'>Capture Image</a><br>";
  html += "<a href='/servo/left'>Biodegradable (Left)</a><br>";
  html += "<a href='/servo/right'>Non-biodegradable (Right)</a><br>";
  html += "<a href='/servo/center'>Center Position</a><br>";
  html += "<a href='/air-quality'>Check Air Quality</a>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

void handleCapture() {
  camera_fb_t * fb = esp_camera_fb_get();
  if (!fb) {
    server.send(500, "text/plain", "Camera capture failed");
    return;
  }
  
  server.sendHeader("Content-Type", "image/jpeg");
  server.sendHeader("Content-Length", String(fb->len));
  server.sendHeader("Connection", "close");
  server.send_P(200, "image/jpeg", (const char *)fb->buf, fb->len);
  
  esp_camera_fb_return(fb);
}

void handleServoLeft() {
  wasteServo.write(SERVO_LEFT);
  Blynk.virtualWrite(V2, SERVO_LEFT);
  Blynk.virtualWrite(V1, "Biodegradable - Left");
  server.send(200, "application/json", "{\"status\":\"moved_left\",\"position\":" + String(SERVO_LEFT) + "}");
  Serial.println("Servo moved to LEFT (Biodegradable)");
}

void handleServoRight() {
  wasteServo.write(SERVO_RIGHT);
  Blynk.virtualWrite(V2, SERVO_RIGHT);
  Blynk.virtualWrite(V1, "Non-biodegradable - Right");
  server.send(200, "application/json", "{\"status\":\"moved_right\",\"position\":" + String(SERVO_RIGHT) + "}");
  Serial.println("Servo moved to RIGHT (Non-biodegradable)");
}

void handleServoCenter() {
  wasteServo.write(SERVO_CENTER);
  Blynk.virtualWrite(V2, SERVO_CENTER);
  server.send(200, "application/json", "{\"status\":\"centered\",\"position\":" + String(SERVO_CENTER) + "}");
  Serial.println("Servo moved to CENTER");
}

void handleAirQuality() {
  DynamicJsonDocument doc(1024);
  doc["air_quality"] = currentAirQuality;
  doc["status"] = currentAirQuality > airQualityThreshold ? "alert" : "normal";
  doc["threshold"] = airQualityThreshold;
  
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void captureAndSend() {
  camera_fb_t * fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed");
    return;
  }
  
  // Here you could encode and send the image to your Python processing system
  Serial.println("Image captured successfully");
  esp_camera_fb_return(fb);
}

void sendAlert() {
  // Sound buzzer
  for (int i = 0; i < 3; i++) {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(200);
    digitalWrite(BUZZER_PIN, LOW);
    delay(200);
  }
  
  // Send Blynk notification
  Blynk.logEvent("air_quality_alert", "High odor levels detected! Air quality: " + String(currentAirQuality) + " ppm");
  Blynk.virtualWrite(V3, "ALERT: High odor detected!");
  
  Serial.println("Alert sent - High odor levels detected!");
}
