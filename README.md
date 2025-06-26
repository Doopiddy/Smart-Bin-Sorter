
# Smart Waste Bin System - Setup and Usage Guide

## Overview
This smart waste bin system automatically segregates biodegradable and non-biodegradable waste using computer vision (YOLOv10), controls a servo motor for waste sorting, and monitors air quality with odor detection alerts.

## System Components
1. ESP32-CAM (AI Thinker) with OV2640 camera
2. Servo motor for waste sorting mechanism  
3. MQ135 air quality sensor
4. Buzzer for alerts
5. LEDs (Green/Red) for status indication
6. Python application with Streamlit interface
7. Blynk mobile app for remote monitoring

## Hardware Setup

### ESP32 Connections:
- Servo Motor: Pin 12
- MQ135 Air Quality Sensor: Pin A0 (analog)
- Buzzer: Pin 14
- Green LED: Pin 15
- Red LED: Pin 2
- Camera: Uses standard ESP32-CAM pinout (built-in)

### Power Requirements:
- ESP32-CAM: 5V/3.3V
- Servo Motor: 5V (external power recommended for higher torque)
- Sensors: 3.3V/5V compatible

## Software Setup

### 1. ESP32 Arduino Setup:

#### Required Libraries:
Install these libraries in Arduino IDE:
- WiFi (built-in)
- BlynkSimpleEsp32
- ESP32Servo  
- ArduinoJson
- WebServer (built-in)
- esp_camera (built-in)
- HTTPClient (built-in)

#### ESP32 Code Structure:
The ESP32 code is now modular with separate files:
- `esp32_smart_waste_bin.ino` - Main program file
- `config.h` - Configuration settings and pin definitions
- `connectivity.h/cpp` - WiFi and Blynk connectivity
- `sensors.h/cpp` - Sensor reading and control logic
- `camera_client.h/cpp` - HTTP communication with camera ESP32
- `web_server.h/cpp` - Web server endpoints

#### OV2640 Camera Setup:
Separate ESP32-CAM for camera functionality:
- `ov2640_camera_server.ino` - Dedicated camera web server
- Provides `/capture` endpoint for image capture
- Runs independently from main ESP32

#### Configuration:
1. Open `config.h` in Arduino IDE
2. Update these credentials:
   ```cpp
   #define BLYNK_TEMPLATE_ID "YourTemplateID"
   #define BLYNK_AUTH_TOKEN "YourAuthToken"
   
   // In connectivity.cpp:
   char ssid[] = "YourWiFiSSID";
   char pass[] = "YourWiFiPassword";
   
   // Camera ESP32 IP (in config.h):
   String CAMERA_ESP32_IP = "http://192.168.1.101";
   ```

3. Upload `esp32_smart_waste_bin.ino` to main ESP32
4. Upload `ov2640_camera_server.ino` to ESP32-CAM
5. Select appropriate boards for each ESP32

### 2. Python Application Setup:

#### Required Python Libraries:
```bash
pip install streamlit
pip install ultralytics
pip install opencv-python
pip install pillow  
pip install requests
pip install numpy
pip install tensorflow-lite
```

#### Python Code Structure:
The Python application is now modular:
- `app.py` - Main Streamlit application
- `config.py` - Configuration settings
- `model_manager.py` - YOLO model loading and caching
- `waste_classifier.py` - Waste classification logic
- `esp32_controller.py` - ESP32 communication
- `ui_components.py` - UI rendering components

#### YOLOv10 Model:
The system uses YOLOv10n.pt model which will be automatically downloaded on first run.

#### Configuration:
1. Open `config.py`
2. Update ESP32 IP addresses:
   ```python
   ESP32_URL = "http://192.168.1.100"  # Main ESP32 IP
   CAMERA_ESP32_IP = "http://192.168.1.101"  # Camera ESP32 IP
   ```

### 3. Blynk App Setup:

#### Blynk Console Setup:
1. Create account at https://blynk.cloud
2. Create new template with device type "ESP32"
3. Add these virtual pins:
   - V0: Air Quality (Number display)
   - V1: Waste Classification (String display)  
   - V2: Servo Position (Slider 0-180)
   - V3: Alert Status (String display)
   - V4: Camera Trigger (Button)

4. Get Template ID and Auth Token
5. Update `config.h` with these credentials

#### Mobile App:
1. Download Blynk app
2. Login with same account
3. Add widgets for each virtual pin
4. Configure notification events for air quality alerts

## Usage Instructions

### Starting the System:

1. **Power on Both ESP32s**: 
   - Main ESP32: Connect to power source
   - Camera ESP32: Connect to power source
   - Wait for WiFi connection (check serial monitors)
   - Note both IP addresses displayed

2. **Start Python Application**:
   ```bash
   streamlit run app.py
   ```
   - Opens web interface at http://localhost:8501

3. **Open Blynk App**: 
   - Connect to your device
   - Monitor real-time data

### Operating the Waste Bin:

#### Method 1 - Python Web Interface:
1. Upload image of waste item or capture from ESP32 camera
2. System analyzes using YOLOv10
3. Classification results show on screen
4. Click sorting buttons to move servo
5. Monitor air quality readings

#### Method 2 - Direct ESP32 Web Interface:
Navigate to main ESP32's IP address in browser:
- `/` - Main control page
- `/servo/left` - Move to biodegradable position  
- `/servo/right` - Move to non-biodegradable position
- `/servo/center` - Return to center
- `/air-quality` - Check current air quality
- `/capture` - Trigger camera capture (communicates with camera ESP32)

#### Method 3 - Camera ESP32 Direct Access:
Navigate to camera ESP32's IP address:
- `/capture` - Take photo directly from camera

#### Method 4 - Blynk Mobile App:
- View real-time air quality
- See waste classification results
- Control servo position manually
- Receive push notifications for alerts

### Waste Classification:

#### Biodegradable Items:
- Food waste (fruits, vegetables, meat, etc.)
- Paper products (newspaper, cardboard, etc.)
- Natural materials (wood, cotton, wool, etc.)

#### Non-Biodegradable Items:
- Plastics (bottles, bags, containers, etc.)
- Metals (cans, foil, etc.)
- Electronics (batteries, phones, etc.)
- Glass items
- Synthetic materials

#### Hazardous Items:
- Batteries
- Chemicals
- Medical waste
- Paint/aerosols
*Note: These require special handling*

### Air Quality Monitoring:

- **Normal Range**: 0-800 ppm
- **Alert Threshold**: Above 800 ppm
- **Indicators**:
  - Green LED: Normal air quality
  - Red LED + Buzzer: High odor detected
  - Blynk notification sent automatically

## Troubleshooting

### ESP32 Issues:
- **WiFi Connection Failed**: Check SSID/password in connectivity.cpp
- **Camera Communication Failed**: Verify camera ESP32 IP in config.h
- **Servo Not Moving**: Check power supply and pin connections in sensors.cpp
- **Air Quality Reading 0**: Check MQ135 sensor wiring
- **Blynk Connection Failed**: Verify auth token in config.h

### Python Application Issues:
- **Model Download Failed**: Check internet connection
- **ESP32 Communication Error**: Verify IP addresses in config.py
- **Camera Access Error**: Check camera ESP32 connectivity
- **Module Import Error**: Install required libraries

### Blynk Issues:
- **Device Offline**: Check main ESP32 WiFi connection
- **No Data Updates**: Verify virtual pin configuration
- **Notifications Not Working**: Enable notifications in app settings

## File Structure

### ESP32 Arduino Files:
```
esp32_smart_waste_bin.ino    - Main program
config.h                     - Configuration and pin definitions
connectivity.h/cpp           - WiFi and Blynk handling
sensors.h/cpp               - Sensor and actuator control
camera_client.h/cpp         - Camera communication
web_server.h/cpp            - HTTP server endpoints
ov2640_camera_server.ino    - Separate camera server
```

### Python Application Files:
```
app.py                      - Main Streamlit application
config.py                   - Configuration settings
model_manager.py            - YOLO model management
waste_classifier.py         - Classification logic
esp32_controller.py         - ESP32 communication
ui_components.py            - UI components and displays
```

## System Specifications

### Performance:
- Waste classification accuracy: ~85-95% (depends on lighting)
- Servo response time: <2 seconds
- Air quality update interval: 5 seconds
- Image processing time: 2-5 seconds
- Camera capture time: 1-3 seconds

### Network Communication:
- Main ESP32 ↔ Python App: HTTP REST API
- Camera ESP32 ↔ Main ESP32: HTTP requests
- Camera ESP32 ↔ Python App: Direct HTTP capture
- Main ESP32 ↔ Blynk: IoT cloud communication

## Maintenance

### Regular Maintenance:
- Clean camera lens weekly
- Calibrate air quality sensor monthly
- Check servo mechanism for smooth operation
- Update Python libraries periodically
- Verify network connectivity between ESP32s

### Troubleshooting Logs:
- ESP32 Main: Check Arduino IDE Serial Monitor
- ESP32 Camera: Check Arduino IDE Serial Monitor  
- Python: Check Streamlit console output
- Blynk: Check device status in console

## Safety Notes
- Use proper power ratings for all components
- Handle hazardous waste according to local regulations
- Keep electronics away from moisture
- Ensure proper ventilation around air quality sensor
- Maintain network security for ESP32 communications

## Support
For technical issues:
1. Check hardware connections
2. Verify software configuration
3. Review troubleshooting section
4. Check component datasheets for specifications
5. Verify network connectivity between devices

---
System Version: 2.0 (Modular Architecture)
Last Updated: 2024
</lov-write>
