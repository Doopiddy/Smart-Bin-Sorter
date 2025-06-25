import cv2
import numpy as np
import requests
import time
from ultralytics import YOLO
from PIL import Image
import json
from threading import Thread
import queue

# Load YOLOv10 model
model = YOLO("yolov10n.pt")

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

class WasteBinController:
    def __init__(self):
        self.processing_queue = queue.Queue()
        self.result_queue = queue.Queue()
        self.air_quality_data = []
        self.classification_history = []
        self.camera_active = False
        self.display_gui = True
    
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
                print("✅ Servo moved LEFT for biodegradable waste")
            elif waste_type == "non-biodegradable":
                response = requests.get(SERVO_RIGHT_URL, timeout=5)
                print("✅ Servo moved RIGHT for non-biodegradable waste")
            elif waste_type == "center":
                response = requests.get(SERVO_CENTER_URL, timeout=5)
                print("✅ Servo moved to CENTER position")
            
            if response.status_code == 200:
                return response.json()
        except requests.exceptions.RequestException as e:
            print(f"❌ Failed to communicate with ESP32: {e}")
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
            print(f"❌ Failed to get air quality: {e}")
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
            print(f"❌ Failed to capture from ESP32 camera: {e}")
            return None
    
    def process_image(self, image):
        """Process image and classify waste"""
        # Convert PIL image to numpy array for YOLO
        image_np = np.array(image)
        
        # Run YOLO detection
        results = model(image_np)
        
        # Process results
        if results and len(results) > 0:
            r = results[0]
            
            if r.boxes is not None and len(r.boxes) > 0:
                # Extract labels and confidences
                labels = [r.names[int(cls)] for cls in r.boxes.cls]
                confidences = r.boxes.conf.tolist()
                
                # Classify waste
                bio_items, non_bio_items, hazardous_items = self.classify_waste(labels, confidences)
                
                # Print results
                print("\n=== Classification Results ===")
                print(f"Biodegradable Items: {len(bio_items)}")
                print(f"Non-Biodegradable Items: {len(non_bio_items)}")
                print(f"Hazardous Items: {len(hazardous_items)}")
                
                # Determine dominant waste type and control servo
                total_items = len(bio_items) + len(non_bio_items) + len(hazardous_items)
                
                if total_items > 0:
                    print("\n=== Sorting Decision ===")
                    
                    if len(hazardous_items) > 0:
                        print("⚠️ HAZARDOUS WASTE DETECTED - Manual handling required!")
                    elif len(bio_items) > len(non_bio_items):
                        print("🌱 Dominant waste type: BIODEGRADABLE")
                        self.send_servo_command("biodegradable")
                    elif len(non_bio_items) > len(bio_items):
                        print("🔴 Dominant waste type: NON-BIODEGRADABLE")
                        self.send_servo_command("non-biodegradable")
                    else:
                        print("⚖️ Equal amounts detected - Manual sorting recommended")
                
                # Return annotated image
                annotated_frame = r.plot()
                return annotated_frame
            
        return None
    
    def run_camera_loop(self):
        """Main camera processing loop"""
        print("Starting camera processing loop...")
        
        # Initialize camera (use 0 for default webcam)
        cap = cv2.VideoCapture(0)
        
        while self.camera_active:
            ret, frame = cap.read()
            if not ret:
                print("Failed to capture frame")
                continue
            
            # Display camera feed
            cv2.imshow("Smart Waste Bin - Camera Feed", frame)
            
            # Check for key presses
            key = cv2.waitKey(1) & 0xFF
            
            # Press 'c' to capture and classify
            if key == ord('c'):
                print("\nCapturing image for classification...")
                image_rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
                pil_image = Image.fromarray(image_rgb)
                
                # Process the image
                annotated_image = self.process_image(pil_image)
                
                if annotated_image is not None:
                    cv2.imshow("Classification Results", annotated_image)
            
            # Press 'a' to check air quality
            elif key == ord('a'):
                print("\nChecking air quality...")
                air_data = self.get_air_quality()
                if air_data:
                    air_quality = air_data.get('air_quality', 0)
                    status = air_data.get('status', 'unknown')
                    
                    print(f"Air Quality: {air_quality} ppm")
                    if status == "alert":
                        print("⚠️ HIGH ODOR LEVELS! Please empty the bin soon")
                    else:
                        print("✅ Air quality normal")
            
            # Press 'q' to quit
            elif key == ord('q'):
                break
        
        # Clean up
        cap.release()
        cv2.destroyAllWindows()
        print("Camera processing stopped")

    def manual_control(self):
        """Manual control interface"""
        print("\n=== Manual Controls ===")
        print("1. Move to Biodegradable (Left)")
        print("2. Move to Non-Biodegradable (Right)")
        print("3. Move to Center")
        print("4. Check Air Quality")
        print("5. Exit")
        
        choice = input("Enter your choice (1-5): ")
        
        if choice == "1":
            self.send_servo_command("biodegradable")
        elif choice == "2":
            self.send_servo_command("non-biodegradable")
        elif choice == "3":
            self.send_servo_command("center")
        elif choice == "4":
            air_data = self.get_air_quality()
            if air_data:
                air_quality = air_data.get('air_quality', 0)
                status = air_data.get('status', 'unknown')
                print(f"\nAir Quality: {air_quality} ppm")
                if status == "alert":
                    print("⚠️ HIGH ODOR LEVELS! Please empty the bin soon")
                else:
                    print("✅ Air quality normal")
        elif choice == "5":
            self.camera_active = False
            self.display_gui = False
        else:
            print("Invalid choice")

    def run(self):
        """Main application loop"""
        print("=== Smart Waste Bin System ===")
        print("Initializing system...")
        
        # Start camera thread
        self.camera_active = True
        camera_thread = Thread(target=self.run_camera_loop)
        camera_thread.start()
        
        # Main loop
        while self.display_gui:
            self.manual_control()
        
        # Wait for camera thread to finish
        camera_thread.join()
        print("System shutdown complete")

if __name__ == "__main__":
    controller = WasteBinController()
    controller.run()
