#ifndef NETWORK_MODULE_H
#define NETWORK_MODULE_H

#include <WiFi.h>
#include <HTTPClient.h>
#include "DisplayModule.h"

class NetworkModule {
public:
    NetworkModule(DisplayModule* display = nullptr);
    
    void begin();
    bool connectWiFi();
    bool isConnected();
    String sendPostRequest(const char* url, const String& jsonPayload);

private:
    DisplayModule* _display;
};

#endif // NETWORK_MODULE_H
