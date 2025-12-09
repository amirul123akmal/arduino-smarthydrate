/*
  ESP32-CAM: capture image, Base64-encode and POST JSON {"image":"<base64>"} to API
  - Modules extracted to .h/.cpp files for better maintainability.
*/

#include <Arduino.h>
#include "config.h"
#include "camera_module.h"
#include "wifi_module.h"
#include "api_module.h"
#include "server_module.h"

unsigned long lastAutoSend = 0;

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println();
  Serial.println("ESP32-CAM Modularized");

  // Connect to WiFi
  connectWiFi();

  // Init Camera
  if (!initCamera()) {
    Serial.println("Camera init failed. Halting.");
    while (true) { delay(1000); }
  }

  printCameraInfo();

  // Start Server
  startWebServer();

  lastAutoSend = millis();
}

void loop() {
  handleServerClient();

  // WiFi reconnect logic embedded in connectWiFi (simple check) 
  // or we can add a ensureConnected check here if needed.
  // The original loop checked status and called ensureWiFiConnected.
  // wifi_module's connectWiFi handles this safely.
  connectWiFi();

  // Optional periodic capture & send
  if (AUTO_SEND_INTERVAL_MS > 0) {
    unsigned long now = millis();
    if ((now - lastAutoSend) >= AUTO_SEND_INTERVAL_MS) {
      lastAutoSend = now;
      Serial.println("Auto-capture triggered.");
      String resp;
      int httpCode;
      if (captureAndSendToApi(API_URL, resp, httpCode)) {
        Serial.printf("Auto-upload succeeded, HTTP %d\n", httpCode);
      } else {
        Serial.printf("Auto-upload failed, HTTP %d, response: %s\n", httpCode, resp.c_str());
      }
    }
  }

  delay(10);
}
