#include "NetworkModule.h"
#include "Config.h"

NetworkModule::NetworkModule(DisplayModule* display) : _display(display) {
}

void NetworkModule::begin() {
    // Optional: Set up static IP if needed here, but usually done before begin/connect
    if (!WiFi.config(LOCAL_IP, GATEWAY, SUBNET, PRIMARY_DNS)) {
        Serial.println("STA Failed to configure");
        if (_display) {
            _display->println("IP Config Failed");
            _display->display();
        }
    }
}

bool NetworkModule::connectWiFi() {
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.print("Connecting to WiFi");
    if (_display) {
        _display->println("Connecting WiFi...");
        _display->display();
    }

    int retry = 0;
    while (WiFi.status() != WL_CONNECTED && retry < 20) {
        delay(500);
        Serial.print(".");
        retry++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nConnected!");
        Serial.print("IP: ");
        Serial.println(WiFi.localIP());
        if (_display) {
            _display->println("WiFi Connected!");
            _display->println(WiFi.localIP().toString());
            _display->display();
        }
        return true;
    } else {
        Serial.println("\nConnection Failed");
        if (_display) {
            _display->println("WiFi Failed");
            _display->display();
        }
        return false;
    }
}

bool NetworkModule::isConnected() {
    return WiFi.status() == WL_CONNECTED;
}

String NetworkModule::sendPostRequest(const char* url, const String& jsonPayload) {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi Disconnected");
        if (_display) {
            _display->clear();
            _display->setCursor(0,0);
            _display->println("WiFi Lost!");
            _display->display();
        }
        return "";
    }

    Serial.println("Sending POST request...");
    if (_display) {
        _display->clear();
        _display->setCursor(0,0);
        _display->println("Capturing...");
        _display->display();
    }

    HTTPClient http;
    http.setTimeout(15000); 
    http.begin(url);
    http.addHeader("Content-Type", "application/json");

    int httpResponseCode = http.POST(jsonPayload);
    String response = "";

    if (httpResponseCode > 0) {
        Serial.print("HTTP Response code: ");
        Serial.println(httpResponseCode);

        if (httpResponseCode == HTTP_CODE_OK) {
            response = http.getString();
            Serial.println("Response received");
        } else {
            if (_display) {
                _display->println("Error: " + String(httpResponseCode));
                _display->display();
            }
        }
    } else {
        Serial.print("Error code: ");
        Serial.println(httpResponseCode);
        if (_display) {
            _display->println("HTTP Error");
            _display->display();
        }
    }

    http.end();
    return response;
}
