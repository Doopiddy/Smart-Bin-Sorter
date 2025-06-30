#include <WiFi.h>
#include <ESP32Servo.h>
#include <ArduinoJson.h>
#include <WebServer.h>

// WiFi credentials
char ssid[] = "YourWiFiSSID";
char pass[] = "YourWiFiPassword";

// Servo pin
#define SERVO_PIN 12

// Servo positions
#define SERVO_CENTER 90
#define SERVO_LEFT   45   // Biodegradable
#define SERVO_RIGHT  135  // Non-biodegradable

Servo wasteServo;
WebServer server(80);

void setup() {
  Serial.begin(115200);
  
  // Initialize servo
  wasteServo.attach(SERVO_PIN);
  wasteServo.write(SERVO_CENTER);
  
  // Connect to WiFi
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("WiFi connected");
  Serial.print("ESP32 IP address: ");
  Serial.println(WiFi.localIP());
  
  // Setup web server routes
  server.on("/", handleRoot);
  server.on("/servo/left", handleServoLeft);
  server.on("/servo/right", handleServoRight);
  server.on("/servo/center", handleServoCenter);
  server.begin();
  
  Serial.println("Smart Waste Bin System Ready!");
}

void loop() {
  server.handleClient();
}

// Web server handlers
void handleRoot() {
  String html = "<html><body>";
  html += "<h1>Smart Waste Bin Control</h1>";
  html += "<a href='/servo/left'>Biodegradable (Left)</a><br>";
  html += "<a href='/servo/right'>Non-biodegradable (Right)</a><br>";
  html += "<a href='/servo/center'>Center Position</a>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

void handleServoLeft() {
  wasteServo.write(SERVO_LEFT);
  server.send(200, "application/json", "{\"status\":\"moved_left\",\"position\":" + String(SERVO_LEFT) + "}");
  Serial.println("Servo moved to LEFT (Biodegradable)");
}

void handleServoRight() {
  wasteServo.write(SERVO_RIGHT);
  server.send(200, "application/json", "{\"status\":\"moved_right\",\"position\":" + String(SERVO_RIGHT) + "}");
  Serial.println("Servo moved to RIGHT (Non-biodegradable)");
}

void handleServoCenter() {
  wasteServo.write(SERVO_CENTER);
  server.send(200, "application/json", "{\"status\":\"centered\",\"position\":" + String(SERVO_CENTER) + "}");
  Serial.println("Servo moved to CENTER");
}
