#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ----------------------- USER CONFIG -----------------------
extern const char* WIFI_SSID;
extern const char* WIFI_PASS;

// Destination API URL (default for input box)
extern const char* API_URL; 

// If true and URL is https://..., will call client.setInsecure() (no cert validation).
extern const bool HTTPS_INSECURE;

// Optional: automatic periodic send interval (ms). Set 0 to disable.
extern const unsigned long AUTO_SEND_INTERVAL_MS; 

// --------------------- Static IP Config --------------------
extern const bool USE_STATIC_IP;

// --------------------- Snap Endpoint Config ----------------
extern const char* SNAP_TARGET_URL;

#endif
