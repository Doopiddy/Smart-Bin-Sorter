#include <WiFi.h>
#include <ESP32Servo.h>
#include <ArduinoJson.h>
#include <WebServer.h>

// WiFi credentials
const char *ssid = "PLDTHOMEFIBRJAWS5";
const char *password = "RVN*7Ven+s_2001";

// === Pin definitions ===
#define GAS_SENSOR_1      32  // G32
#define GAS_SENSOR_2      23  // G23
#define SERVO_1_PIN       33  // G33
#define SERVO_2_PIN       4   // G4
#define ECHO_1            12  // G12
#define TRIG_1            13  // G13
#define ECHO_2            19  // G19
#define TRIG_2            18  // G18
#define LCD_SDA           27  // G27
#define LCD_SCL           14  // G14

// === Constants ===
#define SERVO_LEFT        45
#define SERVO_RIGHT       135
#define SERVO_CENTER      90
#define VCC               3.3
#define RL                10.0
#define RS_AIR            20.0
#define CALIBRATION_SAMPLES 50

Servo servo1;
Servo servo2;
WebServer server(80);

float rsAir1 = RS_AIR;
float rsAir2 = RS_AIR;

void setup() {
  Serial.begin(115200);

  pinMode(GAS_SENSOR_1, INPUT);
  pinMode(GAS_SENSOR_2, INPUT);
  pinMode(TRIG_1, OUTPUT);
  pinMode(ECHO_1, INPUT);
  pinMode(TRIG_2, OUTPUT);
  pinMode(ECHO_2, INPUT);

  servo1.attach(SERVO_1_PIN);
  servo2.attach(SERVO_2_PIN);
  servo1.write(SERVO_CENTER);
  servo2.write(SERVO_CENTER);

  Serial.println("Calibrating MQ-135 sensors...");
  delay(10000);
  rsAir1 = calibrateMQ135(GAS_SENSOR_1);
  rsAir2 = calibrateMQ135(GAS_SENSOR_2);
  Serial.println("Calibration done.");

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("WiFi connected!");
  Serial.print("Device IP: ");
  Serial.println(WiFi.localIP());

  server.on("/", handleRoot);
  server.on("/servo/rotate", handleServoRotate);
  server.on("/air-quality", handleAirQuality);
  server.on("/distance", handleDistance);
  server.on("/ip", handleIP);
  server.begin();

  Serial.println("System Ready!");
}

void loop() {
  server.handleClient();
}

// === Handlers ===
void handleRoot() {
  server.send(200, "text/plain", "ESP32 Smart Wastebin is Running");
}

void handleIP() {
  String ip = WiFi.localIP().toString();
  server.send(200, "application/json", "{\"ip\":\"" + ip + "\"}");
}

void handleServoRotate() {
  servo1.write(SERVO_LEFT);
  servo2.write(SERVO_RIGHT);
  delay(1000);
  servo1.write(SERVO_CENTER);
  servo2.write(SERVO_CENTER);
  server.send(200, "text/plain", "Servos rotated");
}

void handleAirQuality() {
  float gas1 = readMQ135(GAS_SENSOR_1, rsAir1);
  float gas2 = readMQ135(GAS_SENSOR_2, rsAir2);
  String json = "{";
  json += "\"sensor_1\":" + String(gas1, 2) + ",";
  json += "\"sensor_2\":" + String(gas2, 2);
  json += "}";
  server.send(200, "application/json", json);
}

void handleDistance() {
  long dist1 = getDistance(TRIG_1, ECHO_1);
  long dist2 = getDistance(TRIG_2, ECHO_2);
  String json = "{";
  json += "\"ultrasonic_1\":" + String(dist1) + ",";
  json += "\"ultrasonic_2\":" + String(dist2);
  json += "}";
  server.send(200, "application/json", json);
}

// === Sensor Logic ===
float calibrateMQ135(int pin) {
  float rsSum = 0;
  for (int i = 0; i < CALIBRATION_SAMPLES; i++) {
    int val = analogRead(pin);
    float voltage = val * (VCC / 4095.0);
    rsSum += RL * (VCC - voltage) / voltage;
    delay(100);
  }
  return rsSum / CALIBRATION_SAMPLES;
}

float readMQ135(int pin, float rsAir) {
  int val = analogRead(pin);
  float voltage = val * (VCC / 4095.0);
  float rs = RL * (VCC - voltage) / voltage;
  return rs / rsAir;
}

long getDistance(int trig, int echo) {
  digitalWrite(trig, LOW);
  delayMicroseconds(2);
  digitalWrite(trig, HIGH);
  delayMicroseconds(10);
  digitalWrite(trig, LOW);
  long duration = pulseIn(echo, HIGH);
  return duration * 0.034 / 2;
}
