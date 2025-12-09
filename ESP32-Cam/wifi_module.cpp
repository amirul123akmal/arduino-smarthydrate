#include "wifi_module.h"
#include "config.h"

// Define the static IP objects here or in main? 
// Let's define them locally since config has valid values, but we need to declare the actual IPAddress objects.
// Since config.h had constants, let's just use the values.
// Wait, in previous step I put IPAddress objects in main. 
// Let's redefine them here for encapsulation, or pass them in.
// Encapsulation is better.

void connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;

  // Re-declare here or include/extern. 
  // For simplicity, hardcode the values from previous step here or use the ones from config if we moved them.
  // I will assume specific values here based on previous context.
  
  // From previous task:
  IPAddress local_IP(10, 79, 237, 66);
  IPAddress gateway(10, 79, 237, 12);
  IPAddress subnet(255, 255, 255, 0);
  IPAddress primaryDNS(8, 8, 8, 8); 

  Serial.printf("Connecting to WiFi SSID: %s\n", WIFI_SSID);

  if (USE_STATIC_IP) {
    if (!WiFi.config(local_IP, gateway, subnet, primaryDNS)) {
      Serial.println("STA Failed to configure");
    } else {
      Serial.println("Static IP configured.");
    }
  }

  WiFi.begin(WIFI_SSID, WIFI_PASS);

  unsigned long start = millis();
  const unsigned long timeout = 20000; // 20 seconds
  while (WiFi.status() != WL_CONNECTED && (millis() - start) < timeout) {
    Serial.print(".");
    delay(500);
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi connected.");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("WiFi connection failed.");
  }
}
