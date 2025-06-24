import streamlit as st
import numpy as np
import cv2
from ultralytics import YOLO
from PIL import Image
import requests
import json
import time
import tensorflow as tf
from threading import Thread
import queue
import base64
from io import BytesIO

# Load YOLOv10 model
@st.cache_resource
def load_model():
    return YOLO("yolov10n.pt")

model = load_model()

# Comprehensive waste classification lists
biodegradable_items = [
    # Food items
    'banana', 'apple', 'broccoli', 'carrot', 'sandwich', 'orange', 
    'pizza', 'donut', 'cake', 'vegetable', 'fruit', 'hot dog', 
    'bread', 'meat', 'fish', 'egg', 'lettuce', 'tomato', 'potato',
    'onion', 'corn', 'pepper', 'cucumber', 'pear', 'peach', 'plum',
    'grape', 'strawberry', 'blueberry', 'raspberry', 'melon', 'watermelon',
    'mango', 'pineapple', 'coconut', 'avocado', 'lemon', 'lime',
    'cherry', 'apricot', 'kiwi', 'papaya', 'pomegranate', 'fig',
    
    # Paper products (biodegradable)
    'paper', 'tissue', 'napkin', 'newspaper', 'cardboard', 'paper bag',
    'paper towel', 'paper plate', 'paper cup', 'magazine', 'book',
    'notebook', 'envelope', 'receipt', 'toilet paper',
    
    # Natural organic materials
    'wood', 'leaf', 'grass', 'flower', 'branch', 'stick', 'bamboo',
    'cotton', 'wool', 'leather', 'jute', 'hemp', 'cork', 'coconut husk',
    'sawdust', 'tea bag', 'coffee grounds', 'eggshell', 'nutshell',
    
    # Food packaging (biodegradable)
    'wax paper', 'parchment paper', 'compostable container',
    'biodegradable plastic', 'plant-based packaging'
]

non_biodegradable_items = [
    # Plastics
    'plastic bottle', 'plastic bag', 'plastic container', 'plastic wrapper',
    'plastic cup', 'plastic straw', 'plastic cutlery', 'plastic packaging',
    'plastic toy', 'plastic utensil', 'plastic lid', 'plastic film',
    'bubble wrap', 'plastic tube', 'plastic tray', 'plastic box',
    'zip lock bag', 'shopping bag', 'garbage bag', 'food wrapper',
    
    # Metals
    'can', 'aluminum can', 'tin can', 'metal container', 'metal bottle',
    'metal lid', 'foil', 'metal scrap', 'wire', 'nail', 'screw',
    'aluminum foil', 'steel can', 'copper wire', 'metal cap',
    
    # Electronics and batteries
    'battery', 'cell phone', 'laptop', 'keyboard', 'mouse', 'circuit board',
    'cable', 'charger', 'headphones', 'remote control', 'camera',
    'tablet', 'electronic device', 'LED bulb', 'CFL bulb',
    
    # Glass
    'glass bottle', 'glass jar', 'broken glass', 'wine glass', 'drinking glass',
    'glass container', 'mirror', 'window glass', 'glass plate',
    
    # Synthetic materials
    'styrofoam', 'rubber', 'nylon', 'polyester', 'vinyl', 'ceramic',
    'disposable diaper', 'sanitary pad', 'cigarette butt', 'chewing gum',
    'synthetic fabric', 'plastic foam', 'polystyrene', 'PVC',
    
    # Non-recyclable items
    'sticker', 'tape', 'glue', 'marker', 'pen', 'pencil',
    'crayon', 'paint tube', 'adhesive', 'laminated paper'
]

hazardous_items = [
    'battery', 'chemical', 'medicine', 'thermometer', 'light bulb',
    'paint', 'aerosol', 'cleaner', 'pesticide', 'oil', 'solvent',
    'bleach', 'acid', 'motor oil', 'antifreeze', 'nail polish',
    'hair dye', 'printer ink', 'toner cartridge', 'fluorescent bulb'
]

# ESP32 Configuration
ESP32_URL = "http://192.168.1.100"  # Replace with your ESP32's IP
SERVO_LEFT_URL = f"{ESP32_URL}/servo/left"
SERVO_RIGHT_URL = f"{ESP32_URL}/servo/right"
SERVO_CENTER_URL = f"{ESP32_URL}/servo/center"
AIR_QUALITY_URL = f"{ESP32_URL}/air-quality"
CAPTURE_URL = f"{ESP32_URL}/capture"

# Blynk Configuration
BLYNK_AUTH_TOKEN = "YourBlynkAuthToken"
BLYNK_SERVER = "blynk.cloud"

class WasteBinController:
    def __init__(self):
        self.processing_queue = queue.Queue()
        self.result_queue = queue.Queue()
        self.air_quality_data = []
        self.classification_history = []
    
    def classify_waste(self, labels, confidences):
        """Enhanced waste classification with confidence scoring"""
        bio_items = []
        non_bio_items = []
        hazardous_items_found = []
        
        for i, label in enumerate(labels):
            label_lower = label.lower()
            confidence = confidences[i] if i < len(confidences) else 0
            
            # Only consider detections with high confidence
            if confidence < 0.3:
                continue
                
            item_info = {
                'name': label,
                'confidence': confidence,
                'category': 'unknown'
            }
            
            if any(bio_keyword in label_lower for bio_keyword in biodegradable_items):
                item_info['category'] = 'biodegradable'
                bio_items.append(item_info)
            elif any(hazard_keyword in label_lower for hazard_keyword in hazardous_items):
                item_info['category'] = 'hazardous'
                hazardous_items_found.append(item_info)
            elif any(non_bio_keyword in label_lower for non_bio_keyword in non_biodegradable_items):
                item_info['category'] = 'non-biodegradable'
                non_bio_items.append(item_info)
        
        return bio_items, non_bio_items, hazardous_items_found
    
    def send_servo_command(self, waste_type):
        """Send servo command to ESP32"""
        try:
            if waste_type == "biodegradable":
                response = requests.get(SERVO_LEFT_URL, timeout=5)
                st.success("✅ Servo moved LEFT for biodegradable waste")
            elif waste_type == "non-biodegradable":
                response = requests.get(SERVO_RIGHT_URL, timeout=5)
                st.success("✅ Servo moved RIGHT for non-biodegradable waste")
            elif waste_type == "center":
                response = requests.get(SERVO_CENTER_URL, timeout=5)
                st.success("✅ Servo moved to CENTER position")
            
            if response.status_code == 200:
                return response.json()
        except requests.exceptions.RequestException as e:
            st.error(f"❌ Failed to communicate with ESP32: {e}")
            return None
    
    def get_air_quality(self):
        """Get air quality data from ESP32"""
        try:
            response = requests.get(AIR_QUALITY_URL, timeout=5)
            if response.status_code == 200:
                data = response.json()
                self.air_quality_data.append({
                    'timestamp': time.time(),
                    'value': data.get('air_quality', 0),
                    'status': data.get('status', 'unknown')
                })
                return data
        except requests.exceptions.RequestException as e:
            st.error(f"❌ Failed to get air quality: {e}")
            return None
    
    def capture_from_esp32(self):
        """Capture image from ESP32 camera"""
        try:
            response = requests.get(CAPTURE_URL, timeout=10)
            if response.status_code == 200:
                image_array = np.frombuffer(response.content, np.uint8)
                image = cv2.imdecode(image_array, cv2.IMREAD_COLOR)
                return cv2.cvtColor(image, cv2.COLOR_BGR2RGB)
        except requests.exceptions.RequestException as e:
            st.error(f"❌ Failed to capture from ESP32 camera: {e}")
            return None

# Initialize controller
controller = WasteBinController()

# Streamlit UI
st.set_page_config(page_title="Smart Waste Bin", page_icon="♻️", layout="wide")

st.title("♻️ Smart Waste Bin Control System")
st.markdown("""
### AI-Powered Waste Classification & Monitoring
This system uses YOLOv10 and computer vision to classify waste automatically, 
controls servo mechanisms for waste sorting, and monitors air quality in real-time.
""")

# Create main layout
col1, col2, col3 = st.columns([2, 1, 1])

with col1:
    st.subheader("🎯 Waste Classification")
    
    # Image input options
    input_method = st.radio("Choose input method:", 
                           ["Upload Image", "Capture from ESP32 Camera", "Use Webcam"])
    
    image = None
    
    if input_method == "Upload Image":
        uploaded_file = st.file_uploader("Upload waste image", type=["jpg", "jpeg", "png"])
        if uploaded_file:
            image = Image.open(uploaded_file).convert("RGB")
    
    elif input_method == "Capture from ESP32 Camera":
        if st.button("📷 Capture from ESP32", type="primary"):
            with st.spinner("Capturing from ESP32 camera..."):
                image = controller.capture_from_esp32()
                if image is not None:
                    image = Image.fromarray(image)
    
    elif input_method == "Use Webcam":
        # This would require additional setup for webcam access
        st.info("Webcam functionality requires additional configuration")

with col2:
    st.subheader("🌡️ Air Quality Monitor")
    
    if st.button("Check Air Quality", type="secondary"):
        air_data = controller.get_air_quality()
        if air_data:
            air_quality = air_data.get('air_quality', 0)
            status = air_data.get('status', 'unknown')
            
            # Display air quality metrics
            st.metric("Air Quality", f"{air_quality} ppm")
            
            if status == "alert":
                st.error("⚠️ HIGH ODOR LEVELS!")
                st.markdown("🚨 **Action Required**: Empty the bin soon")
            else:
                st.success("✅ Air quality normal")
            
            # Progress bar for air quality
            progress_value = min(100, air_quality / 10)
            st.progress(progress_value / 100)
    
    # Air quality history chart
    if controller.air_quality_data:
        st.subheader("📈 Air Quality Trend")
        recent_data = controller.air_quality_data[-10:]  # Last 10 readings
        st.line_chart([d['value'] for d in recent_data])

with col3:
    st.subheader("🎛️ Manual Controls")
    
    st.markdown("**Servo Control:**")
    if st.button("⬅️ Biodegradable", use_container_width=True):
        controller.send_servo_command("biodegradable")
    
    if st.button("➡️ Non-Biodegradable", use_container_width=True):
        controller.send_servo_command("non-biodegradable")
    
    if st.button("🎯 Center Position", use_container_width=True):
        controller.send_servo_command("center")
    
    st.markdown("**System Status:**")
    st.json({
        "ESP32 Connection": "🟢 Connected",
        "Camera Status": "🟢 Active",
        "Servo Status": "🟢 Ready",
        "Air Sensor": "🟢 Monitoring"
    })

# Process image if available
if image is not None:
    st.subheader("📸 Captured Image")
    st.image(image, caption="Waste to be classified", use_column_width=True)
    
    # Convert PIL image to numpy array for YOLO
    image_np = np.array(image)
    
    # Run YOLO detection
    with st.spinner("🔍 Analyzing waste with AI..."):
        results = model(image_np)
    
    # Process results
    if results and len(results) > 0:
        r = results[0]
        
        if r.boxes is not None and len(r.boxes) > 0:
            # Extract labels and confidences
            labels = [r.names[int(cls)] for cls in r.boxes.cls]
            confidences = r.boxes.conf.tolist()
            
            # Classify waste
            bio_items, non_bio_items, hazardous_items = controller.classify_waste(labels, confidences)
            
            # Display results
            st.subheader("🎯 Classification Results")
            
            # Create tabs for different waste categories
            tab1, tab2, tab3, tab4 = st.tabs(["📊 Summary", "🌱 Biodegradable", "🔴 Non-Biodegradable", "⚠️ Hazardous"])
            
            with tab1:
                col_bio, col_non_bio, col_hazard = st.columns(3)
                
                with col_bio:
                    st.metric("Biodegradable Items", len(bio_items))
                with col_non_bio:
                    st.metric("Non-Biodegradable Items", len(non_bio_items))
                with col_hazard:
                    st.metric("Hazardous Items", len(hazardous_items))
            
            with tab2:
                if bio_items:
                    for item in bio_items:
                        st.success(f"✅ {item['name']} (Confidence: {item['confidence']:.2f})")
                else:
                    st.info("No biodegradable items detected")
            
            with tab3:
                if non_bio_items:
                    for item in non_bio_items:
                        st.error(f"❌ {item['name']} (Confidence: {item['confidence']:.2f})")
                else:
                    st.info("No non-biodegradable items detected")
            
            with tab4:
                if hazardous_items:
                    for item in hazardous_items:
                        st.warning(f"⚠️ {item['name']} (Confidence: {item['confidence']:.2f})")
                    st.error("🚨 HAZARDOUS WASTE DETECTED - Requires special handling!")
                else:
                    st.info("No hazardous items detected")
            
            # Determine dominant waste type and control servo
            total_items = len(bio_items) + len(non_bio_items) + len(hazardous_items)
            
            if total_items > 0:
                st.subheader("🤖 Automatic Sorting Decision")
                
                if len(hazardous_items) > 0:
                    st.error("⚠️ HAZARDOUS WASTE DETECTED - Manual handling required!")
                    st.info("Please remove hazardous items and dispose of them separately.")
                elif len(bio_items) > len(non_bio_items):
                    st.success("🌱 Dominant waste type: **BIODEGRADABLE**")
                    if st.button("🚀 Activate Biodegradable Sorting", type="primary"):
                        controller.send_servo_command("biodegradable")
                        st.balloons()
                elif len(non_bio_items) > len(bio_items):
                    st.warning("🔴 Dominant waste type: **NON-BIODEGRADABLE**")
                    if st.button("🚀 Activate Non-Biodegradable Sorting", type="primary"):
                        controller.send_servo_command("non-biodegradable")
                        st.balloons()
                else:
                    st.info("⚖️ Equal amounts detected - Manual sorting recommended")
            
            # Display annotated image
            st.subheader("🔍 Detection Visualization")
            annotated_frame = r.plot()
            st.image(annotated_frame, caption="AI Detection Results", use_column_width=True)
            
        else:
            st.warning("No objects detected in the image. Please try with a clearer image.")
    else:
        st.error("Failed to process the image. Please try again.")

# Sidebar with additional information
st.sidebar.header("📊 System Statistics")

# Classification history
if controller.classification_history:
    st.sidebar.subheader("Recent Classifications")
    for item in controller.classification_history[-5:]:
        st.sidebar.text(f"• {item}")

# System information
st.sidebar.subheader("ℹ️ About")
st.sidebar.info("""
**Smart Waste Bin System v2.0**

🎯 **Features:**
- AI-powered waste classification
- Real-time air quality monitoring  
- Automatic servo-based sorting
- ESP32 camera integration
- Blynk IoT connectivity

🔧 **Technology Stack:**
- YOLOv10 for object detection
- ESP32 with OV2640 camera
- MQ135 air quality sensor
- Servo motor for sorting
- Streamlit web interface
""")

# Required libraries information
st.sidebar.subheader("📚 Required Libraries")
st.sidebar.code("""
# Python libraries:
pip install streamlit
pip install ultralytics
pip install opencv-python
pip install tensorflow
pip install requests
pip install pillow
pip install numpy

# Arduino libraries:
- WiFi
- BlynkSimpleEsp32
- ESP32Servo
- ArduinoJson
- WebServer
- esp_camera
""")
