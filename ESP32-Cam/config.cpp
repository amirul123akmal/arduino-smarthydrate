#include "config.h"

// ----------------------- USER CONFIG -----------------------
const char* WIFI_SSID = "N";
const char* WIFI_PASS = "1234567899";

// Destination API URL (default for input box)
const char* API_URL = "https://example.com/api/upload"; 

// If true and URL is https://..., will call client.setInsecure() (no cert validation).
const bool HTTPS_INSECURE = true;

// Optional: automatic periodic send interval (ms). Set 0 to disable.
const unsigned long AUTO_SEND_INTERVAL_MS = 0; 

// --------------------- Static IP Config --------------------
const bool USE_STATIC_IP = true;

// --------------------- Snap Endpoint Config ----------------
const char* SNAP_TARGET_URL = "http://192.168.137.1:5000/upload-base64";
