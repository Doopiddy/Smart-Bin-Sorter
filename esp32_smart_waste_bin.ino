#include <WiFi.h>
#include <ESP32Servo.h>
#include <WebServer.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// WiFi credentials
const char* ssid = "PLDTHOMEFIBRJAWS5";
const char* password = "RVN*7Ven+s_2001";

// Pin definitions
#define SERVO1_PIN        33
#define SERVO2_PIN        4
#define GAS_SENSOR1_PIN   32
#define GAS_SENSOR2_PIN   23
#define ULTRASONIC1_ECHO  12
#define ULTRASONIC1_TRIG  13
#define ULTRASONIC2_ECHO  19
#define ULTRASONIC2_TRIG  18

// LCD Configuration
#define LCD_ADDR          0x27
#define LCD_COLS          16
#define LCD_ROWS          2

// Servo positions
#define SERVO_CENTER      90
#define SERVO_LEFT1       45
#define SERVO_RIGHT1      135
#define SERVO_LEFT2       135
#define SERVO_RIGHT2      45

// MQ-135 Constants
#define VCC                 3.3
#define RL                  10.0
#define CALIBRATION_SAMPLES 50
#define CALIBRATION_DELAY   100

Servo servo1;
Servo servo2;
WebServer server(80);
LiquidCrystal_I2C lcd(LCD_ADDR, LCD_COLS, LCD_ROWS);

// Sensor values
float rsAirCalibrated1 = 0.0;
float rsAirCalibrated2 = 0.0;
float currentAirQuality1 = 0.0;
float currentAirQuality2 = 0.0;
long currentDistance1 = 0;
long currentDistance2 = 0;

String localIP;

// === SETUP ===
void setup() {
  Serial.begin(115200);

  pinMode(GAS_SENSOR1_PIN, INPUT);
  pinMode(GAS_SENSOR2_PIN, INPUT);
  pinMode(ULTRASONIC1_TRIG, OUTPUT);
  pinMode(ULTRASONIC1_ECHO, INPUT);
  pinMode(ULTRASONIC2_TRIG, OUTPUT);
  pinMode(ULTRASONIC2_ECHO, INPUT);

  servo1.attach(SERVO1_PIN);
  servo2.attach(SERVO2_PIN);
  centerServos();

  Wire.begin();
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("System Booting...");
  lcd.setCursor(0, 1);
  lcd.print("Please wait.");

  // Warm-up and Calibration
  Serial.println("MQ-135 warm-up (10s)...");
  delay(10000);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Calibrating MQ1");
  rsAirCalibrated1 = calibrateMQ135(GAS_SENSOR1_PIN);
  lcd.setCursor(0, 1);
  lcd.print("MQ1 Rs: " + String(rsAirCalibrated1, 2));
  delay(1000);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Calibrating MQ2");
  rsAirCalibrated2 = calibrateMQ135(GAS_SENSOR2_PIN);
  lcd.setCursor(0, 1);
  lcd.print("MQ2 Rs: " + String(rsAirCalibrated2, 2));
  delay(2000);

  connectToWiFi();
  setupWebServer();

  Serial.println("System Ready!");
  updateLCD();
}

void loop() {
  server.handleClient();
  updateSensors();
  updateLCD();
  delay(500);
}

// === WiFi ===
void connectToWiFi() {
  WiFi.begin(ssid, password);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Connecting WiFi");
  lcd.setCursor(0, 1);
  lcd.print("Status: ...");

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(1000);
    lcd.setCursor(attempts % 16, 1);
    lcd.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    localIP = WiFi.localIP().toString();
    Serial.println("WiFi connected: " + localIP);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WiFi Connected!");
    lcd.setCursor(0, 1);
    lcd.print("IP: " + localIP);
    delay(2000);
  } else {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WiFi Failed!");
    lcd.setCursor(0, 1);
    lcd.print("Check WiFi");
    delay(5000);
    ESP.restart();
  }
}

// === Web Server ===
void setupWebServer() {
  server.on("/", handleRoot);
  server.on("/left", handleLeft);
  server.on("/right", handleRight);
  server.on("/center", handleCenter);
  server.on("/air-quality", handleAirQuality);
  server.on("/distance", handleDistance);
  server.on("/status", handleStatus);
  server.begin();
  Serial.println("Web server started.");
}

// === LCD ===
void updateLCD() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("IP:" + localIP);
  lcd.setCursor(0, 1);
  lcd.print("AQ1:" + String(currentAirQuality1, 2) + " D1:" + String(currentDistance1));
}

// === Sensors ===
void updateSensors() {
  currentAirQuality1 = readMQ135(GAS_SENSOR1_PIN, rsAirCalibrated1);
  currentAirQuality2 = readMQ135(GAS_SENSOR2_PIN, rsAirCalibrated2);
  currentDistance1 = getDistance(ULTRASONIC1_TRIG, ULTRASONIC1_ECHO);
  currentDistance2 = getDistance(ULTRASONIC2_TRIG, ULTRASONIC2_ECHO);

  Serial.print("AQ1: " + String(currentAirQuality1, 2));
  Serial.print(" | AQ2: " + String(currentAirQuality2, 2));
  Serial.print(" | D1: " + String(currentDistance1) + "cm");
  Serial.print(" | D2: " + String(currentDistance2) + "cm\n");
}

float readMQ135(int pin, float rsAirCalibratedValue) {
  int analogValue = analogRead(pin);
  float voltage = analogValue * (VCC / 4095.0);
  float rs = RL * (VCC - voltage) / voltage;
  return rs / rsAirCalibratedValue;
}

float calibrateMQ135(int pin) {
  float rsSum = 0;
  for (int i = 0; i < CALIBRATION_SAMPLES; i++) {
    int analogValue = analogRead(pin);
    float voltage = analogValue * (VCC / 4095.0);
    float rs = RL * (VCC - voltage) / voltage;
    rsSum += rs;
    delay(CALIBRATION_DELAY);
  }
  return rsSum / CALIBRATION_SAMPLES;
}

long getDistance(int trigPin, int echoPin) {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  long duration = pulseIn(echoPin, HIGH);
  return duration * 0.034 / 2;
}

// === Servo Control ===
void centerServos() {
  servo1.write(SERVO_CENTER);
  servo2.write(SERVO_CENTER);
  Serial.println("Servos centered.");
}

void moveLeft() {
  servo1.write(SERVO_LEFT1);
  servo2.write(SERVO_LEFT2);
  Serial.println("Servos moved left.");
}

void moveRight() {
  servo1.write(SERVO_RIGHT1);
  servo2.write(SERVO_RIGHT2);
  Serial.println("Servos moved right.");
}

// === Web Handlers ===
void handleRoot() {
  String html = "<html><head><meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<style>body{font-family:Arial;text-align:center;background:#f0f0f0;}a{padding:10px;margin:5px;display:inline-block;background:#007bff;color:white;text-decoration:none;border-radius:5px;}a:hover{background:#0056b3;}</style></head><body>";
  html += "<h1>Waste Sorting System</h1>";
  html += "<p><a href='/left'>Move Left</a><a href='/center'>Center</a><a href='/right'>Move Right</a></p>";
  html += "<p>Air Quality 1: " + String(currentAirQuality1, 2) + "</p>";
  html += "<p>Air Quality 2: " + String(currentAirQuality2, 2) + "</p>";
  html += "<p>Distance 1: " + String(currentDistance1) + " cm</p>";
  html += "<p>Distance 2: " + String(currentDistance2) + " cm</p>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

void handleLeft() {
  moveLeft();
  server.send(200, "text/plain", "Moved Left");
}

void handleRight() {
  moveRight();
  server.send(200, "text/plain", "Moved Right");
}

void handleCenter() {
  centerServos();
  server.send(200, "text/plain", "Centered");
}

void handleAirQuality() {
  String response = "{\"sensor1\":" + String(currentAirQuality1) + ",\"sensor2\":" + String(currentAirQuality2) + "}";
  server.send(200, "application/json", response);
}

void handleDistance() {
  String response = "{\"sensor1\":" + String(currentDistance1) + ",\"sensor2\":" + String(currentDistance2) + "}";
  server.send(200, "application/json", response);
}

void handleStatus() {
  String status = "{\"ip\":\"" + localIP + "\",\"status\":\"running\"}";
  server.send(200, "application/json", status);
}
