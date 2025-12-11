#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "mbedtls/base64.h"

// --- Configuration ---
// WiFi Settings - STATIC IP (User to adjust)
IPAddress local_IP(192, 168, 137, 65); // Example Static IP
IPAddress gateway(192, 168, 137, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress primaryDNS(8, 8, 8, 8);   // Optional

const char* ssid = "N";
const char* password = "1234567899";

// Server Settings
const char* serverUrl = "http://192.168.137.50/snap"; // User to adjust

// Pin Definitions
const int BUTTON_PIN = 17;

// Display Settings
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Global Variables
bool isSending = false;

void setup() {
  Serial.begin(115200);
  
  // 1. Setup Input
  pinMode(BUTTON_PIN, INPUT_PULLDOWN); // Assumes active HIGH (connect VCC -> Switch -> Pin 17) // If using internal pullup and switching to ground, use INPUT_PULLUP and check for LOW

  // 2. Setup Display
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { 
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Booting...");
  display.display();

  // 3. Setup WiFi with Static IP
  if (!WiFi.config(local_IP, gateway, subnet, primaryDNS)) {
    Serial.println("STA Failed to configure");
    display.println("IP Config Failed");
    display.display();
  }

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  display.println("Connecting WiFi...");
  display.display();

  int retry = 0;
  while (WiFi.status() != WL_CONNECTED && retry < 20) {
    delay(500);
    Serial.print(".");
    retry++;
  }

  display.clearDisplay();
  display.setCursor(0,0);
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConnected!");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
    display.println("WiFi Connected!");
    display.println(WiFi.localIP());
  } else {
    Serial.println("\nConnection Failed");
    display.println("WiFi Failed");
  }
  display.println("Ready. Press G17.");
  display.display();
}

void loop() {
  // Simple debounce/state check could be added, here using simple digitalRead logic
  // Wait for button press
  if (digitalRead(BUTTON_PIN) == HIGH && !isSending) {
    isSending = true; // Primitive debounce/lock
    sendPostRequest();
    // specific delay to prevent multi-trigger
    delay(1000); 
    isSending = false;
    
    // Restore Ready State
    display.clearDisplay();
    display.setCursor(0,0);
    display.println(WiFi.localIP());
    display.println("Ready. Press G17.");
    display.display();
  }
}

void sendPostRequest() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi Disconnected");
    display.clearDisplay();
    display.setCursor(0,0);
    display.println("WiFi Lost!");
    display.display();
    return;
  }

  Serial.println("Sending POST request...");
  display.clearDisplay();
  display.setCursor(0,0);
  display.println("Sending POST...");
  display.display();

  HTTPClient http;
  http.begin(serverUrl);
  http.addHeader("Content-Type", "application/json");

  // Send empty JSON or specific payload if needed
  int httpResponseCode = http.POST("{}");

  if (httpResponseCode > 0) {
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);

    if (httpResponseCode == HTTP_CODE_OK) {
      String response = http.getString();
      Serial.println("Response received (truncated):");
      Serial.println(response.substring(0, 100) + "...");

      processResponse(response);
    } else {
      display.println("Error: " + String(httpResponseCode));
      display.display();
    }
  } else {
    Serial.print("Error code: ");
    Serial.println(httpResponseCode);
    display.println("HTTP Error");
    display.display();
  }

  http.end();
}

void processResponse(String& payload) {
  // Manual JSON parsing to avoid ArduinoJson dependency
  // We look for "image": "<base64_string>"
  
  int keyIndex = payload.indexOf("\"image\"");
  if (keyIndex == -1) {
    keyIndex = payload.indexOf("'image'"); // flexible quote check
  }

  if (keyIndex == -1) {
    Serial.println("No 'image' key found");
    display.println("No Image in JSON");
    display.display();
    return;
  }

  // Find the colon after "image"
  int colonIndex = payload.indexOf(":", keyIndex);
  if (colonIndex == -1) return;

  // Find the opening quote of the value
  int startQuote = payload.indexOf("\"", colonIndex);
  if (startQuote == -1) startQuote = payload.indexOf("'", colonIndex);
  if (startQuote == -1) return;

  // We found the start of the data
  Serial.println("Image Data Detected.");
  display.println("Img Data Rcvd!");
  
  // Calculate approximate length (up to next quote)
  int endQuote = payload.indexOf("\"", startQuote + 1);
  if (endQuote == -1) endQuote = payload.indexOf("'", startQuote + 1);
  
  long dataLen = 0;
  if (endQuote != -1) {
    dataLen = endQuote - (startQuote + 1);
  } else {
    // If we can't find the end, just assume it goes to the end of the string - 1 (closing brace)
    dataLen = payload.length() - (startQuote + 1) - 1; 
  }

  display.print("Len: ");
  display.println(dataLen);
  display.println("Too big to show");
  display.display();

  delay(2000);
}
