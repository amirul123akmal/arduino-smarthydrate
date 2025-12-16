#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include <IPAddress.h>

// --- WiFi Settings - STATIC IP ---
// (User to adjust)
const IPAddress LOCAL_IP(192, 168, 137, 65);
const IPAddress GATEWAY(192, 168, 137, 1);
const IPAddress SUBNET(255, 255, 255, 0);
const IPAddress PRIMARY_DNS(8, 8, 8, 8);

const char* const WIFI_SSID = "N";
const char* const WIFI_PASSWORD = "1234567899";

// --- Server Settings ---
const char* const SERVER_URL = "http://192.168.137.50/snap";

// --- Pin Definitions ---
const int BUTTON_PIN = 17;

// --- Display Settings ---
const int SCREEN_WIDTH = 128;
const int SCREEN_HEIGHT = 64;
const int OLED_RESET = -1;
const int SCREEN_ADDRESS = 0x3C;

#endif // CONFIG_H
