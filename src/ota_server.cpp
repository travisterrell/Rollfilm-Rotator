#include "ota_server.h"

#if HAS_OTA_SUPPORT

// Global objects for OTA functionality
AsyncWebServer server(OTA_PORT);
AsyncWebSocket ws("/ws");
unsigned long ota_progress_millis = 0;
unsigned long status_update_millis = 0;

// OTA Progress callbacks
void onOTAStart() {
  Serial.println("OTA update started!");
  // Stop processor during OTA to avoid interference
  StopCycleBrake();
}

void onOTAProgress(size_t current, size_t final) {
  // Log progress every second
  if (millis() - ota_progress_millis > 1000) {
    ota_progress_millis = millis();
    float progress = ((float)current / (float)final) * 100.0f;
    Serial.printf("OTA Progress: %.1f%% (%u/%u bytes)\n", progress, current, final);
  }
}

void onOTAEnd(bool success) {
  if (success) {
    Serial.println("OTA update completed successfully! Rebooting...");
  } else {
    Serial.println("OTA update failed!");
  }
}

// WebSocket event handler
void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, 
               void *arg, uint8_t *data, size_t len) {
  switch(type) {
    case WS_EVT_CONNECT:
      Serial.printf("WebSocket client connected: %u\n", client->id());
      break;
    case WS_EVT_DISCONNECT:
      Serial.printf("WebSocket client disconnected: %u\n", client->id());
      break;
    case WS_EVT_DATA: {
      AwsFrameInfo *info = (AwsFrameInfo*)arg;
      if(info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
        String command = "";
        for(size_t i = 0; i < len; i++) {
          command += (char)data[i];
        }
        Serial.printf("WebSocket command received: %s\n", command.c_str());
        
        // Handle remote serial commands
        if(command == "start") StartContinuousCycle();
        else if(command == "stop") StopCycleBrake();
        else if(command == "coast") StopCycleCoast();
        // Add more commands as needed
      }
      break;
    }
    default:
      break;
  }
}

// Broadcast status updates to all connected WebSocket clients
void broadcastStatus() {
  if(ws.count() > 0) {
    String status = "{";
    status += "\"uptime\":" + String(millis()) + ",";
    status += "\"heap\":" + String(ESP.getFreeHeap()) + ",";
    status += "\"wifi_rssi\":" + String(WiFi.RSSI());
    status += "}";
    ws.textAll(status);
  }
}

void setupWiFi() {
  Serial.printf("Connecting to WiFi network: %s", WIFI_SSID);
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  unsigned long startTime = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - startTime) < WIFI_TIMEOUT_MS) {
    delay(500);
    Serial.print(".");
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("");
    Serial.printf("WiFi connected! IP address: %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("OTA interface available at: http://%s/update\n", WiFi.localIP().toString().c_str());
    Serial.printf("Live dashboard available at: http://%s/\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println("");
    Serial.println("WiFi connection failed - OTA updates unavailable");
  }
}

void setupOTA() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected - skipping OTA setup");
    return;
  }
  
  // Live dashboard with real-time updates
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/html", getDashboardHTML());
  });
  
  // JSON API endpoints
  server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *request) {
    String json = "{";
    json += "\"uptime\":" + String(millis()) + ",";
    json += "\"heap\":" + String(ESP.getFreeHeap()) + ",";
    json += "\"wifi_rssi\":" + String(WiFi.RSSI()) + ",";
    json += "\"ip\":\"" + WiFi.localIP().toString() + "\"";
    json += "}";
    request->send(200, "application/json", json);
  });
  
  // WebSocket setup
  ws.onEvent(onWsEvent);
  server.addHandler(&ws);
  
  // Initialize ElegantOTA
  ElegantOTA.begin(&server);
  ElegantOTA.onStart(onOTAStart);
  ElegantOTA.onProgress(onOTAProgress);
  ElegantOTA.onEnd(onOTAEnd);
  
  server.begin();
  Serial.println("Async OTA server started with live dashboard");
}

void serviceOTA() {
  // Send status updates every 2 seconds
  if (millis() - status_update_millis > 2000) {
    status_update_millis = millis();
    broadcastStatus();
  }
  
  // Clean up WebSocket connections
  ws.cleanupClients();
  
  ElegantOTA.loop();   // Handle OTA updates
}

#endif // HAS_OTA_SUPPORT