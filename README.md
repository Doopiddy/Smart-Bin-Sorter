# Smart-Bin-Sorter

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

#### Configuration:
1. Open `esp32_smart_waste_bin.ino` in Arduino IDE
2. Update these credentials:
   ```cpp
   #define BLYNK_TEMPLATE_ID "YourTemplateID"
   #define BLYNK_AUTH_TOKEN "YourAuthToken"
   char ssid[] = "YourWiFiSSID";
   char pass[] = "YourWiFiPassword";
   ```

3. Select Board: "AI Thinker ESP32-CAM"
4. Upload the code to ESP32

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

#### YOLOv10 Model:
The system uses YOLOv10n.pt model which will be automatically downloaded on first run.

#### Configuration:
1. Open `smart_waste_bin_python.py`
2. Update ESP32 IP address:
   ```python
   ESP32_URL = "http://192.168.1.100"  # Replace with your ESP32's IP
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
5. Update ESP32 code with these credentials

#### Mobile App:
1. Download Blynk app
2. Login with same account
3. Add widgets for each virtual pin
4. Configure notification events for air quality alerts

## Usage Instructions

### Starting the System:

1. **Power on ESP32**: 
   - Connect to power source
   - Wait for WiFi connection (check serial monitor)
   - Note the IP address displayed

2. **Start Python Application**:
   ```bash
   streamlit run smart_waste_bin_python.py
   ```
   - Opens web interface at http://localhost:8501

3. **Open Blynk App**: 
   - Connect to your device
   - Monitor real-time data

### Operating the Waste Bin:

#### Method 1 - Python Web Interface:
1. Upload image of waste item
2. System analyzes using YOLOv10
3. Classification results show on screen
4. Click "Activate Bin" to move servo
5. Monitor air quality readings

#### Method 2 - Direct ESP32 Web Interface:
Navigate to ESP32's IP address in browser:
- `/` - Main control page
- `/capture` - Take photo
- `/servo/left` - Move to biodegradable position  
- `/servo/right` - Move to non-biodegradable position
- `/servo/center` - Return to center
- `/air-quality` - Check current air quality

#### Method 3 - Blynk Mobile App:
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
- **WiFi Connection Failed**: Check SSID/password
- **Camera Init Failed**: Check camera connections
- **Servo Not Moving**: Verify power supply and pin connections
- **Air Quality Reading 0**: Check MQ135 sensor wiring

### Python Application Issues:
- **Model Download Failed**: Check internet connection
- **ESP32 Communication Error**: Verify IP address and network
- **Camera Access Error**: Check camera permissions
- **Module Import Error**: Install required libraries

### Blynk Issues:
- **Device Offline**: Check ESP32 WiFi connection
- **No Data Updates**: Verify virtual pin configuration
- **Notifications Not Working**: Enable notifications in app settings

## System Specifications

### Performance:
- Waste classification accuracy: ~85-95% (depends on lighting)
- Servo response time: <2 seconds
- Air quality update interval: 5 seconds
- Image processing time: 2-5 seconds

### Limitations:
- Requires good lighting for accurate classification
- Single item detection works best
- WiFi connection required for remote features
- Air quality sensor needs 24-48 hours warmup for accuracy

## Maintenance

### Regular Maintenance:
- Clean camera lens weekly
- Calibrate air quality sensor monthly
- Check servo mechanism for smooth operation
- Update Python libraries periodically

### Troubleshooting Logs:
- ESP32: Check Arduino IDE Serial Monitor
- Python: Check Streamlit console output
- Blynk: Check device status in console

## Safety Notes
- Use proper power ratings for all components
- Handle hazardous waste according to local regulations
- Keep electronics away from moisture
- Ensure proper ventilation around air quality sensor

## Support
For technical issues:
1. Check hardware connections
2. Verify software configuration
3. Review troubleshooting section
4. Check component datasheets for specifications

---
System Version: 1.0
Last Updated: 2024
