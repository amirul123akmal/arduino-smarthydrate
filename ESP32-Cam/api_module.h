#ifndef API_MODULE_H
#define API_MODULE_H

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include "esp_camera.h"
#include "mbedtls/base64.h"

bool captureAndSendToApi(const char* targetUrl, String &outResponse, int &outHttpCode, String *outBase64Image = NULL);

#endif
