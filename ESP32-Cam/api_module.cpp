#include "api_module.h"
#include "config.h"

bool captureAndSendToApi(const char* targetUrl, String &outResponse, int &outHttpCode) {
  outResponse = "";
  outHttpCode = 0;

  if (WiFi.status() != WL_CONNECTED) {
    outResponse = "WiFi not connected";
    return false;
  }

  // Flush the buffer to ensure a fresh frame
  camera_fb_t * fb = esp_camera_fb_get();
  if (fb) {
    esp_camera_fb_return(fb);
    fb = NULL;
  }

  // Real capture
  fb = esp_camera_fb_get();
  if (!fb) {
    outResponse = "Camera capture failed";
    return false;
  }

  size_t src_len = fb->len;
  size_t required_b64_len = 0;
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

  String json;
  json.reserve(actual_b64_len + 32);
  json += "{\"image\":\"";
  json += (char*)b64_buf;
  json += "\"}";

  esp_camera_fb_return(fb);

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
  http.setConnectTimeout(10000); 
  http.setTimeout(30000); 

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
