
#include "esp_camera.h"
#include <WiFi.h>
#include <WebServer.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

// WiFi credentials (same network as main ESP32)
const char* ssid = "YourWiFiSSID";
const char* password = "YourWiFiPassword";

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

// Flash LED pin
#define FLASH_LED_PIN     4

WebServer server(80);

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();
  
  // Disable brownout detector
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
  
  // Initialize flash LED
  pinMode(FLASH_LED_PIN, OUTPUT);
  digitalWrite(FLASH_LED_PIN, LOW);
  
  // Camera configuration
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
  
  // Frame size and quality settings
  if(psramFound()){
    config.frame_size = FRAMESIZE_UXGA; // High resolution
    config.jpeg_quality = 10;           // High quality (lower number = better quality)
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_SVGA; // Medium resolution
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }
  
  // Initialize camera
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }
  
  // Camera sensor settings for better image quality
  sensor_t * s = esp_camera_sensor_get();
  if (s->id.PID == OV2640_PID) {
    s->set_brightness(s, 0);     // -2 to 2
    s->set_contrast(s, 0);       // -2 to 2
    s->set_saturation(s, 0);     // -2 to 2
    s->set_special_effect(s, 0); // 0 to 6 (0-No Effect, 1-Negative, 2-Grayscale, 3-Red Tint, 4-Green Tint, 5-Blue Tint, 6-Sepia)
    s->set_whitebal(s, 1);       // 0 = disable , 1 = enable
    s->set_awb_gain(s, 1);       // 0 = disable , 1 = enable
    s->set_wb_mode(s, 0);        // 0 to 4 - if awb_gain enabled (0 - Auto, 1 - Sunny, 2 - Cloudy, 3 - Office, 4 - Home)
    s->set_exposure_ctrl(s, 1);  // 0 = disable , 1 = enable
    s->set_aec2(s, 0);           // 0 = disable , 1 = enable
    s->set_ae_level(s, 0);       // -2 to 2
    s->set_aec_value(s, 300);    // 0 to 1200
    s->set_gain_ctrl(s, 1);      // 0 = disable , 1 = enable
    s->set_agc_gain(s, 0);       // 0 to 30
    s->set_gainceiling(s, (gainceiling_t)0);  // 0 to 6
    s->set_bpc(s, 0);            // 0 = disable , 1 = enable
    s->set_wpc(s, 1);            // 0 = disable , 1 = enable
    s->set_raw_gma(s, 1);        // 0 = disable , 1 = enable
    s->set_lenc(s, 1);           // 0 = disable , 1 = enable
    s->set_hmirror(s, 0);        // 0 = disable , 1 = enable
    s->set_vflip(s, 0);          // 0 = disable , 1 = enable
    s->set_dcw(s, 1);            // 0 = disable , 1 = enable
    s->set_colorbar(s, 0);       // 0 = disable , 1 = enable
  }
  
  // Connect to WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("Camera ESP32 IP address: ");
  Serial.println(WiFi.localIP());
  
  // Setup web server routes
  server.on("/", handleRoot);
  server.on("/capture", handleCapture);
  server.on("/stream", handleStream);
  server.on("/settings", handleSettings);
  server.on("/flash/on", handleFlashOn);
  server.on("/flash/off", handleFlashOff);
  server.on("/status", handleStatus);
  
  // Start server
  server.begin();
  Serial.println("Camera server started");
  Serial.println("Available endpoints:");
  Serial.println("  /        - Camera web interface");
  Serial.println("  /capture - Capture single image");
  Serial.println("  /stream  - Live video stream");
  Serial.println("  /flash/on - Turn flash on");
  Serial.println("  /flash/off - Turn flash off");
  Serial.println("  /status  - Camera status");
}

void loop() {
  server.handleClient();
}

void handleRoot() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>OV2640 Camera Server</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; text-align: center; }
        .container { max-width: 800px; margin: 0 auto; }
        button { padding: 10px 20px; margin: 5px; font-size: 16px; cursor: pointer; }
        .capture-btn { background-color: #4CAF50; color: white; border: none; border-radius: 4px; }
        .flash-btn { background-color: #ff9800; color: white; border: none; border-radius: 4px; }
        .stream-btn { background-color: #2196F3; color: white; border: none; border-radius: 4px; }
        #image-container { margin: 20px 0; }
        #captured-image { max-width: 100%; height: auto; border: 2px solid #ddd; }
        .info { background-color: #f0f0f0; padding: 10px; margin: 10px 0; border-radius: 4px; }
    </style>
</head>
<body>
    <div class="container">
        <h1>📷 OV2640 Camera Server</h1>
        <div class="info">
            <p><strong>Camera IP:</strong> )rawliteral" + WiFi.localIP().toString() + R"rawliteral(</p>
            <p><strong>Status:</strong> <span id="status">Ready</span></p>
        </div>
        
        <div>
            <button class="capture-btn" onclick="captureImage()">📸 Capture Image</button>
            <button class="flash-btn" onclick="toggleFlash()">💡 Toggle Flash</button>
            <button class="stream-btn" onclick="openStream()">🎥 Live Stream</button>
        </div>
        
        <div id="image-container">
            <img id="captured-image" src="" alt="Captured image will appear here" style="display: none;">
        </div>
        
        <div class="info">
            <h3>API Endpoints:</h3>
            <p><code>/capture</code> - Get JPEG image</p>
            <p><code>/stream</code> - Live MJPEG stream</p>
            <p><code>/flash/on</code> - Turn flash on</p>
            <p><code>/flash/off</code> - Turn flash off</p>
        </div>
    </div>

    <script>
        let flashOn = false;
        
        function captureImage() {
            document.getElementById('status').textContent = 'Capturing...';
            const img = document.getElementById('captured-image');
            img.src = '/capture?' + new Date().getTime();
            img.style.display = 'block';
            img.onload = function() {
                document.getElementById('status').textContent = 'Image captured successfully';
            };
            img.onerror = function() {
                document.getElementById('status').textContent = 'Failed to capture image';
            };
        }
        
        function toggleFlash() {
            flashOn = !flashOn;
            const endpoint = flashOn ? '/flash/on' : '/flash/off';
            fetch(endpoint)
                .then(response => response.text())
                .then(data => {
                    document.getElementById('status').textContent = flashOn ? 'Flash ON' : 'Flash OFF';
                });
        }
        
        function openStream() {
            window.open('/stream', '_blank', 'width=800,height=600');
        }
    </script>
</body>
</html>
)rawliteral";
  
  server.send(200, "text/html", html);
}

void handleCapture() {
  // Turn on flash briefly for better image quality
  digitalWrite(FLASH_LED_PIN, HIGH);
  delay(100); // Brief flash
  
  camera_fb_t * fb = esp_camera_fb_get();
  if (!fb) {
    digitalWrite(FLASH_LED_PIN, LOW);
    server.send(500, "text/plain", "Camera capture failed");
    Serial.println("Camera capture failed");
    return;
  }
  
  digitalWrite(FLASH_LED_PIN, LOW); // Turn off flash
  
  server.sendHeader("Content-Type", "image/jpeg");
  server.sendHeader("Content-Length", String(fb->len));
  server.sendHeader("Connection", "close");
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send_P(200, "image/jpeg", (const char *)fb->buf, fb->len);
  
  esp_camera_fb_return(fb);
  Serial.println("Image captured and sent");
}

void handleStream() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>Live Camera Stream</title>
    <style>
        body { margin: 0; display: flex; justify-content: center; align-items: center; min-height: 100vh; background: #000; }
        img { max-width: 100%; max-height: 100vh; }
    </style>
</head>
<body>
    <img src="/stream_feed" alt="Live Stream">
</body>
</html>
)rawliteral";
  server.send(200, "text/html", html);
}

void handleFlashOn() {
  digitalWrite(FLASH_LED_PIN, HIGH);
  server.send(200, "text/plain", "Flash ON");
  Serial.println("Flash turned ON");
}

void handleFlashOff() {
  digitalWrite(FLASH_LED_PIN, LOW);
  server.send(200, "text/plain", "Flash OFF");
  Serial.println("Flash turned OFF");
}

void handleSettings() {
  String json = "{";
  json += "\"framesize\":\"" + String(esp_camera_sensor_get()->status.framesize) + "\",";
  json += "\"quality\":\"" + String(esp_camera_sensor_get()->status.quality) + "\",";
  json += "\"brightness\":\"" + String(esp_camera_sensor_get()->status.brightness) + "\",";
  json += "\"contrast\":\"" + String(esp_camera_sensor_get()->status.contrast) + "\",";
  json += "\"flash_status\":\"" + String(digitalRead(FLASH_LED_PIN) ? "ON" : "OFF") + "\"";
  json += "}";
  
  server.send(200, "application/json", json);
}

void handleStatus() {
  String json = "{";
  json += "\"camera_ready\":true,";
  json += "\"ip_address\":\"" + WiFi.localIP().toString() + "\",";
  json += "\"wifi_connected\":" + String(WiFi.status() == WL_CONNECTED ? "true" : "false") + ",";
  json += "\"flash_status\":\"" + String(digitalRead(FLASH_LED_PIN) ? "ON" : "OFF") + "\",";
  json += "\"free_heap\":" + String(ESP.getFreeHeap()) + ",";
  json += "\"uptime\":" + String(millis()) + "";
  json += "}";
  
  server.send(200, "application/json", json);
}
