#include "Config.h"
#include "DisplayModule.h"
#include "NetworkModule.h"
#include "JpegDecoderModule.h"

// --- Global Objects ---
DisplayModule display(SCREEN_WIDTH, SCREEN_HEIGHT, OLED_RESET);
NetworkModule network(&display);

bool isSending = false;

void setup() {
  Serial.begin(115200);
  
  // 1. Setup Input
  pinMode(BUTTON_PIN, INPUT_PULLDOWN); 

  // 2. Setup Display
  if(!display.begin(SCREEN_ADDRESS)) { 
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }
  display.clear();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Booting...");
  display.display();

  // 4. Setup Network
  network.begin(); // IP Config
  network.connectWiFi();

  // Ready Message
  display.clear();
  display.setCursor(0,0);
  if (network.isConnected()) {
    display.println("Ready. Press G17.");
  } else {
    display.println("WiFi Failed. Retry?");
  }
  display.display();
}

void loop() {
  // Wait for button press
  if (digitalRead(BUTTON_PIN) == HIGH && !isSending) {
    isSending = true; 
    
    // Send Request
    String response = network.sendPostRequest(SERVER_URL, "{}");
    
    delay(4000); 
    display.clear();
    display.setCursor(0,0);
    display.println(WiFi.localIP());
    display.println("Ready. Press G17.");
    display.display();
  }
}
