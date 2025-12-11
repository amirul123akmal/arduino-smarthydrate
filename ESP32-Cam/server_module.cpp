#include "server_module.h"
#include "api_module.h"
#include "config.h"
#include "esp_camera.h"
#include <WiFi.h>

WebServer server(80);

void handleRoot();
void handleCaptureEndpoint();
void handleSnapEndpoint();
void handleStream();

void startWebServer() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/capture", HTTP_GET, handleCaptureEndpoint);
  server.on("/snap", HTTP_POST, handleSnapEndpoint);
  server.on("/stream", HTTP_GET, handleStream);

  server.begin();
  Serial.println("HTTP server started.");
}

void handleServerClient() {
  server.handleClient();
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

void handleSnapEndpoint() {
  String apiResp;
  int httpCode = 0;
  String base64Image;
  bool ok = captureAndSendToApi(SNAP_TARGET_URL, apiResp, httpCode, &base64Image);

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
    reply += ", \"image\": \"" + base64Image + "\"";
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
    reply += ", \"image\": \"" + base64Image + "\"";
    reply += "}";
    server.send(500, "application/json", reply);
  }
}

void handleStream() {
  WiFiClient client = server.client();
  String boundary = "frameboundary";
  String header = "HTTP/1.1 200 OK\r\n";
  header += "Content-Type: multipart/x-mixed-replace; boundary=" + boundary + "\r\n";
  header += "Connection: close\r\n\r\n";

  if (!client.print(header)) {
    client.stop();
    return;
  }

  while (client.connected()) {
    camera_fb_t * fb = esp_camera_fb_get();
    if (!fb) {
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
    delay(100);
  }
  client.stop();
}
