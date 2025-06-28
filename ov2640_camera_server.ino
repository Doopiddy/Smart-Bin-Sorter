#include <WiFi.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

const char* ssid = "PLDTFIBRJAWS5";
const char* password = "RVN*Ven+s_2001";

void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); // Disable brownout detector
  Serial.begin(115200);
  delay(2000);
  
  Serial.println("\n\nStarting ESP32-CAM Test");
  Serial.println("======================");

  // WiFi Connection
  WiFi.begin(ssid, password);
  Serial.printf("Connecting to %s", ssid);
  
  unsigned long startTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startTime < 20000) {
    Serial.print(".");
    delay(500);
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("\nWiFi FAILED. Possible causes:");
    Serial.println("- Wrong credentials");
    Serial.println("- Weak signal (use 2.4GHz)");
    Serial.println("- Insufficient power (use 5V/2A)");
    while (1); // Halt
  }

  Serial.println("\nWiFi Connected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
}

void loop() {
  delay(1000);
}
