/*
  ESP32-CAM: capture image, Base64-encode and POST JSON {"image":"<base64>"} to API
  - AI-Thinker module pinout
  - Endpoints:
      /        -> status page
      /capture -> capture and POST to API
      /stream  -> MJPEG stream
  - Configure WIFI_SSID, WIFI_PASS, API_URL below
  - Uses mbedtls/base64 for encoding
  - Uses WiFiClient for HTTPS (optional insecure mode)
*/

#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include "esp_camera.h"
#include "mbedtls/base64.h"
#include <WiFiClientSecure.h>

// ----------------------- USER CONFIG -----------------------
const char* WIFI_SSID = "M";
const char* WIFI_PASS = "1234567899";

// Destination API URL (http:// or https://)
const char* API_URL = "http:"; // <-- change to your API

// If true and API_URL is https://..., will call client.setInsecure() (no cert validation).
// For production, replace with setCACert() and your CA certificate.
const bool HTTPS_INSECURE = true;

// Optional: automatic periodic send interval (ms). Set 0 to disable.
const unsigned long AUTO_SEND_INTERVAL_MS = 0; // e.g., 60000 for every 60s
// -----------------------------------------------------------

// --------------------- Static IP Config --------------------
const bool USE_STATIC_IP = true;
IPAddress local_IP(10, 79, 237, 66);
IPAddress gateway(10, 79, 237, 12);
IPAddress subnet(255, 255, 255, 0);
IPAddress primaryDNS(8, 8, 8, 8); // Optional
// -----------------------------------------------------------

// AI-Thinker Camera Pin Map
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27

#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

WebServer server(80);
unsigned long lastAutoSend = 0;

// Forward declarations
bool initCamera();
void startWebServer();
void handleRoot();
void handleCaptureEndpoint();
void handleStream();
bool captureAndSendToApi(const char* targetUrl, String &outResponse, int &outHttpCode);
void ensureWiFiConnected();
void printCameraInfo();

// --------------------- Setup & Loop -------------------------
void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println();
  Serial.println("ESP32-CAM Base64 Upload (JSON {\"image\":\"...\"})");

  WiFi.mode(WIFI_STA);
  WiFi.setHostname("esp32cam-b64");
  ensureWiFiConnected();

  if (!initCamera()) {
    Serial.println("Camera init failed. Halting.");
    while (true) { delay(1000); }
  }

  printCameraInfo();
  startWebServer();

  lastAutoSend = millis();
}

void loop() {
  server.handleClient();

  // WiFi reconnect logic
  if (WiFi.status() != WL_CONNECTED) {
    ensureWiFiConnected();
  }

  // Optional periodic capture & send
  if (AUTO_SEND_INTERVAL_MS > 0) {
    unsigned long now = millis();
    if ((now - lastAutoSend) >= AUTO_SEND_INTERVAL_MS) {
      lastAutoSend = now;
      Serial.println("Auto-capture triggered.");
      String resp;
      int httpCode;
      // Use default API_URL for auto-send
      if (captureAndSendToApi(API_URL, resp, httpCode)) {
        Serial.printf("Auto-upload succeeded, HTTP %d\n", httpCode);
      } else {
        Serial.printf("Auto-upload failed, HTTP %d, response: %s\n", httpCode, resp.c_str());
      }
    }
  }

  delay(10);
}

// --------------------- Camera init -------------------------
bool initCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  // Choose a reasonably sized frame to keep payloads manageable
  config.frame_size = FRAMESIZE_VGA; // 640x480
  config.jpeg_quality = 12; // 0 = best, 63 = worst. Increase number to reduce image size.
  config.fb_count = 1;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x\n", err);
    return false;
  }
  return true;
}

// --------------------- Web server --------------------------
void startWebServer() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/capture", HTTP_GET, handleCaptureEndpoint);
  server.on("/stream", HTTP_GET, handleStream);

  server.begin();
  Serial.println("HTTP server started.");
  Serial.println("  /        (status)");
  Serial.println("  /capture (capture now and POST to API)");
  Serial.println("  /stream  (MJPEG stream)");
}

void handleRoot() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>ESP32-CAM Capture</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { font-family: sans-serif; text-align: center; margin: 0; padding: 20px; }
    input, button { padding: 10px; font-size: 16px; margin: 10px; width: 80%; max-width: 400px; }
    #output { background: #f4f4f4; padding: 10px; text-align: left; white-space: pre-wrap; margin: 20px auto; width: 90%; border: 1px solid #ddd; min-height: 100px; }
    h3 { margin-bottom: 5px; }
  </style>
</head>
<body>
  <h3>ESP32-CAM Capture</h3>
  <p>IP: )rawliteral";
  html += WiFi.localIP().toString();
  html += R"rawliteral(</p>
  
  <input type="text" id="url" placeholder="Enter API URL (e.g. http://192.168.1.5:5000/upload)" value=")rawliteral";
  html += API_URL;
  html += R"rawliteral(">
  <br>
  <button onclick="capture()">Capture & Send</button>
  
  <div id="output">Output will appear here...</div>
  <p><a href="/stream">View Stream</a></p>

  <script>
    function capture() {
      var urlInput = document.getElementById("url").value;
      var out = document.getElementById("output");
      out.innerText = "Capturing and sending...";
      
      var endpoint = "/capture?url=" + encodeURIComponent(urlInput);
      
      fetch(endpoint)
        .then(response => response.json())
        .then(data => {
          out.innerText = JSON.stringify(data, null, 2);
        })
        .catch(err => {
          out.innerText = "Error: " + err;
        });
    }
  </script>
</body>
</html>
)rawliteral";

  server.send(200, "text/html", html);
}

void handleCaptureEndpoint() {
  String targetUrl = String(API_URL);
  if (server.hasArg("url") && server.arg("url") != "") {
    targetUrl = server.arg("url");
  }

  String apiResp;
  int httpCode = 0;
  bool ok = captureAndSendToApi(targetUrl.c_str(), apiResp, httpCode);

  String reply;
  if (ok) {
    reply = "{\"success\":true, \"http_code\":";
    reply += String(httpCode);
    reply += ", \"api_response\":";
    String esc = apiResp;
    esc.replace("\\", "\\\\");
    esc.replace("\"", "\\\"");
    esc.replace("\n", "\\n");
    esc.replace("\r", "\\r");
    reply += "\"" + esc + "\"";
    reply += "}";
    server.send(200, "application/json", reply);
  } else {
    reply = "{\"success\":false, \"http_code\":";
    reply += String(httpCode);
    reply += ", \"api_response\":";
    String esc = apiResp;
    esc.replace("\\", "\\\\");
    esc.replace("\"", "\\\"");
    esc.replace("\n", "\\n");
    esc.replace("\r", "\\r");
    reply += "\"" + esc + "\"";
    reply += "}";
    server.send(500, "application/json", reply);
  }
}

// MJPEG stream endpoint (simple)
void handleStream() {
  WiFiClient client = server.client();
  Serial.println("Client connected to /stream");
  String boundary = "frameboundary";
  String header = "HTTP/1.1 200 OK\r\n";
  header += "Content-Type: multipart/x-mixed-replace; boundary=" + boundary + "\r\n";
  header += "Connection: close\r\n\r\n";

  if (!client.print(header)) {
    Serial.println("Failed to send stream header");
    client.stop();
    return;
  }

  while (client.connected()) {
    camera_fb_t * fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Camera capture failed while streaming");
      break;
    }

    String partHeader = "--" + boundary + "\r\n";
    partHeader += "Content-Type: image/jpeg\r\n";
    partHeader += "Content-Length: " + String(fb->len) + "\r\n\r\n";

    if (!client.print(partHeader)) {
      esp_camera_fb_return(fb);
      break;
    }

    size_t written = client.write(fb->buf, fb->len);
    client.print("\r\n");
    esp_camera_fb_return(fb);

    if (!client.connected()) break;
    delay(30);
  }
  client.stop();
  Serial.println("Client disconnected from /stream");
}

// --------------------- Capture & send ----------------------
// Captures a frame, Base64-encodes it, sends JSON {"image":"<base64>"} to targetUrl.
// Returns true on success (HTTP 2xx). outResponse contains API response body or error.
bool captureAndSendToApi(const char* targetUrl, String &outResponse, int &outHttpCode) {
  outResponse = "";
  outHttpCode = 0;

  if (WiFi.status() != WL_CONNECTED) {
    outResponse = "WiFi not connected";
    return false;
  }

  camera_fb_t * fb = esp_camera_fb_get();
  if (!fb) {
    outResponse = "Camera capture failed";
    return false;
  }

  size_t src_len = fb->len;
  size_t required_b64_len = 0;
  // Query required base64 length. The function returns MBEDTLS_ERR_BASE64_BUFFER_TOO_SMALL
  // but required_b64_len will be set to needed size.
  mbedtls_base64_encode(NULL, 0, &required_b64_len, fb->buf, src_len);

  unsigned char * b64_buf = (unsigned char*) malloc(required_b64_len + 1);
  if (!b64_buf) {
    esp_camera_fb_return(fb);
    outResponse = "Out of memory for base64 buffer";
    return false;
  }

  size_t actual_b64_len = 0;
  int ret = mbedtls_base64_encode(b64_buf, required_b64_len + 1, &actual_b64_len, fb->buf, src_len);
  if (ret != 0) {
    free(b64_buf);
    esp_camera_fb_return(fb);
    outResponse = "Base64 encoding failed";
    return false;
  }
  b64_buf[actual_b64_len] = 0; // null-terminate

  // Build JSON exactly: {"image":"<base64>"}
  String json;
  json.reserve(actual_b64_len + 32);
  json += "{\"image\":\"";
  json += (char*)b64_buf;
  json += "\"}";

  // Free camera frame ASAP to reclaim heap
  esp_camera_fb_return(fb);

  // Prepare HTTP client (HTTP or HTTPS)
  // Prepare HTTP client (HTTP or HTTPS)
  bool useHttps = String(targetUrl).startsWith("https://");
  WiFiClient *tcpClient = NULL;
  WiFiClientSecure *secureClient = NULL;

  if (useHttps) {
    secureClient = new WiFiClientSecure();
    if (!secureClient) {
      free(b64_buf);
      outResponse = "Failed to allocate WiFiClient";
      return false;
    }
    if (HTTPS_INSECURE) secureClient->setInsecure();
    tcpClient = secureClient;
  } else {
    tcpClient = new WiFiClient();
    if (!tcpClient) {
      free(b64_buf);
      outResponse = "Failed to allocate WiFiClient";
      return false;
    }
  }

  HTTPClient http;
  http.setConnectTimeout(10000); // 10s
  http.setTimeout(30000); // 30s

  bool beginOk = http.begin(*tcpClient, targetUrl);
  if (!beginOk) {
    free(b64_buf);
    delete tcpClient;
    outResponse = "HTTP begin() failed";
    return false;
  }

  http.addHeader("Content-Type", "application/json");

  Serial.printf("Posting JSON payload (%u bytes) to %s\n", (unsigned int)json.length(), targetUrl);
  int httpCode = http.POST((uint8_t*)json.c_str(), json.length());
  outHttpCode = httpCode;

  if (httpCode > 0) {
    outResponse = http.getString();
    Serial.printf("HTTP returned code %d\n", httpCode);
    Serial.printf("Response: %s\n", outResponse.c_str());
  } else {
    outResponse = String("HTTP POST failed: ") + http.errorToString(httpCode).c_str();
    Serial.printf("HTTP POST failed, error: %s\n", outResponse.c_str());
  }

  http.end();
  delete tcpClient;
  free(b64_buf);

  return (httpCode >= 200 && httpCode < 300);
}

// --------------------- WiFi helpers ------------------------
void ensureWiFiConnected() {
  if (WiFi.status() == WL_CONNECTED) return;

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
    Serial.println("WiFi connection failed. Will retry in background.");
  }
}

void printCameraInfo() {
  sensor_t * s = esp_camera_sensor_get();
  if (s) {
    Serial.printf("Camera PID: %d, MIDH: %d, MIDL: %d\n", s->id.PID, s->id.MIDH, s->id.MIDL);
    Serial.printf("Frame size: %d, quality: %d\n", s->status.framesize, s->status.quality);
  }
}
