
#define BLYNK_TEMPLATE_ID "YourTemplateID"
#define BLYNK_DEVICE_NAME "Smart Waste Bin"
#define BLYNK_AUTH_TOKEN "YourAuthToken"

#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <ESP32Servo.h>
#include <ArduinoJson.h>
#include <WebServer.h>
#include <HTTPClient.h>

// WiFi credentials
char ssid[] = "YourWiFiSSID";
char pass[] = "YourWiFiPassword";

// Servo and sensor pins
#define SERVO_PIN         12
#define AIR_QUALITY_PIN   A0  // MQ135 sensor (GPIO36 for ESP32)
#define BUZZER_PIN        14
#define LED_GREEN         15
#define LED_RED           2

// Servo positions
#define SERVO_CENTER      90
#define SERVO_LEFT        45   // Biodegradable
#define SERVO_RIGHT       135  // Non-biodegradable

// Camera ESP32 configuration
String CAMERA_ESP32_IP = "192.168.1.101";  // IP of the camera ESP32
String CAMERA_CAPTURE_URL = "http://" + CAMERA_ESP32_IP + "/capture";

Servo wasteServo;
WebServer server(80);
HTTPClient http;

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
  
  // Initialize pins
  pinMode(AIR_QUALITY_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_RED, OUTPUT);
  
  // Initialize servo
  wasteServo.attach(SERVO_PIN);
  wasteServo.write(SERVO_CENTER);
  
  // Connect to WiFi and Blynk
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("WiFi connected");
  Serial.print("Main ESP32 IP address: ");
  Serial.println(WiFi.localIP());
  
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
  
  // Setup web server routes
  server.on("/", handleRoot);
  server.on("/servo/left", handleServoLeft);
  server.on("/servo/right", handleServoRight);
  server.on("/servo/center", handleServoCenter);
  server.on("/air-quality", handleAirQuality);
  server.on("/capture-request", handleCaptureRequest);
  server.on("/status", handleStatus);
  server.begin();
  
  Serial.println("Smart Waste Bin System Ready!");
  Serial.println("Camera ESP32 should be at: " + CAMERA_ESP32_IP);
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
    requestCameraCapture();
  }
}

// Web server handlers
void handleRoot() {
  String html = "<html><body>";
  html += "<h1>Smart Waste Bin Control - Main ESP32</h1>";
  html += "<p>Air Quality: " + String(currentAirQuality) + " ppm</p>";
  html += "<p>Camera ESP32 IP: " + CAMERA_ESP32_IP + "</p>";
  html += "<a href='/capture-request'>Request Camera Capture</a><br>";
  html += "<a href='/servo/left'>Biodegradable (Left)</a><br>";
  html += "<a href='/servo/right'>Non-biodegradable (Right)</a><br>";
  html += "<a href='/servo/center'>Center Position</a><br>";
  html += "<a href='/air-quality'>Check Air Quality</a><br>";
  html += "<a href='/status'>System Status</a>";
  html += "</body></html>";
  server.send(200, "text/html", html);
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
  doc["led_green"] = digitalRead(LED_GREEN);
  doc["led_red"] = digitalRead(LED_RED);
  
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void handleCaptureRequest() {
  String result = requestCameraCapture();
  DynamicJsonDocument doc(1024);
  
  if (result == "success") {
    doc["status"] = "success";
    doc["message"] = "Camera capture request sent successfully";
    doc["camera_url"] = "http://" + CAMERA_ESP32_IP + "/capture";
  } else {
    doc["status"] = "error";
    doc["message"] = "Failed to communicate with camera ESP32";
    doc["error"] = result;
  }
  
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void handleStatus() {
  DynamicJsonDocument doc(1024);
  doc["main_esp32_ip"] = WiFi.localIP().toString();
  doc["camera_esp32_ip"] = CAMERA_ESP32_IP;
  doc["air_quality"] = currentAirQuality;
  doc["servo_position"] = wasteServo.read();
  doc["wifi_connected"] = WiFi.status() == WL_CONNECTED;
  doc["blynk_connected"] = Blynk.connected();
  doc["led_status"]["green"] = digitalRead(LED_GREEN);
  doc["led_status"]["red"] = digitalRead(LED_RED);
  
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

String requestCameraCapture() {
  http.begin(CAMERA_CAPTURE_URL);
  int httpResponseCode = http.GET();
  
  if (httpResponseCode > 0) {
    String response = http.getString();
    Serial.println("Camera capture requested successfully");
    http.end();
    return "success";
  } else {
    Serial.println("Error communicating with camera ESP32: " + String(httpResponseCode));
    http.end();
    return "HTTP Error: " + String(httpResponseCode);
  }
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
</lov-write>

